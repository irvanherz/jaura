#ifndef test
#define final

#include <malloc.h>
#include <microhttpd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <vector>

namespace Jaura {
using namespace std;

class HttpRequestContext {
 public:
  time_t timestamp;
  string url;
  map<string, string> query;
  map<string, string> params;
  map<string, string> headers;
  string body;
  string method;

  HttpRequestContext() {
    url = "";
    body = "";
    method = "GET";
    timestamp = time(NULL);
  }
};

class HttpResponseContext {
 public:
  unsigned short status;
  map<string, string> headers;
  string body;

  HttpResponseContext() {
    this->status = 200;
    this->body = "";
    this->headers["X-Powered-By"] = "Jaura";
  }
};

class HttpContext {
 private:
  vector<void *> allocations;

 public:
  HttpRequestContext *request;
  HttpResponseContext *response;

  HttpContext() {
    this->request = new HttpRequestContext();
    this->response = new HttpResponseContext();
  }

  void *malloc(size_t size) {
    void *addr = malloc(size);
    this->allocations.push_back(addr);
    return addr;
  }

  ~HttpContext() {
    for (auto addr : this->allocations) {
      free(addr);
    }
    delete this->request;
    delete this->response;
  }
};

class HttpController;

class HttpRoute {
 public:
  string method;
  string pattern;         // raw url pattern
  regex matcher;          // processed url pattern
  vector<string> params;  // url param names
  HttpController *controller;
  std::function<void(HttpContext *)> handler;

  HttpRoute(string method, string pattern, HttpController *controller,
            std::function<void(HttpContext *)> handler) {
    this->method = method;
    this->pattern = pattern;
    this->controller = controller;
    this->handler = handler;

    // extract url components separated by slash
    vector<string> components;
    string pattern_copy = pattern;
    char *component = strtok(pattern_copy.data(), "/");
    while (component != NULL) {
      if (strlen(component) > 0) {
        components.push_back(string(component));
      }
      component = strtok(NULL, "/");
    }

    // build regex based on components
    string re = "";
    for (auto &component : components) {
      smatch matchedElements;
      bool isUrlParam = regex_match(component, matchedElements,
                                    regex("^\\:([a-zA-Z0-9_]+)(\\(.*\\))?$"));
      if (isUrlParam) {
        // add to reference
        string paramName = string(matchedElements[1].str());
        this->params.push_back(paramName);
        // check if it has custom regex
        if (matchedElements[2].length() > 0) {
          string customRegex = matchedElements[2].str();
          re = re + "/" + customRegex;
        } else {
          re = re + "/([^/]+)";
        }
      } else {
        re = re + "/" + component;
      }
    }
    re = "^" + re + "/?$";
    this->matcher = regex(re);
  }

  bool resolve(HttpContext *context) {
    string url = context->request->url;
    smatch paramValues;

    bool isMethodMatched = context->request->method == this->method;
    bool isUrlMatched = regex_match(url, paramValues, this->matcher);
    bool isMatched = isMethodMatched && isUrlMatched;

    if (isMatched) {
      int numMatchedParams = paramValues.size() - 1;  // only submatches
      for (int i = 0; i < numMatchedParams; i++) {
        string paramName = this->params[i];
        string paramValue = paramValues[i + 1].str();
        context->request->params[paramName] = paramValue;
      }
      this->handler(context);
    }
    return isMatched;
  }
};

class HttpActionFilter {
 public:
  void before(HttpContext *context) {}
  void after(HttpContext *context) {}
};

class HttpController {
 public:
  vector<HttpRoute> routes;

  void registerRoute(HttpRoute route) { routes.push_back(route); }

  bool resolve(HttpContext *context) {
    for (auto &route : this->routes) {
      bool isResolved = route.resolve(context);
      if (isResolved) return true;
    }
    return false;
  }
};

class HttpModule {
 public:
  vector<HttpController *> controllers;

  void registerController(HttpController *controller) {
    this->controllers.push_back(controller);
  }

  bool resolve(HttpContext *context) {
    for (auto &controller : this->controllers) {
      bool isResolved = controller->resolve(context);
      if (isResolved) return true;
    }
    return false;
  }
};

class HttpApplication {
 private:
  MHD_Daemon *daemon;

  static MHD_Result request_query_iterator(void *cls, MHD_ValueKind kind,
                                           const char *key, const char *value) {
    HttpContext *ctx = (HttpContext *)cls;
    ctx->request->query[string(key)] = string(value);
    return MHD_YES;
  }

  static MHD_Result request_header_iterator(void *cls, MHD_ValueKind kind,
                                            const char *key,
                                            const char *value) {
    HttpContext *ctx = (HttpContext *)cls;
    ctx->request->headers[string(key)] = string(value);
    return MHD_YES;
  }

  static MHD_Result daemon_handler(void *cls, struct MHD_Connection *connection,
                                   const char *url, const char *method,
                                   const char *version, const char *upload_data,
                                   size_t *upload_data_size, void **con_cls) {
    HttpApplication *app = (HttpApplication *)cls;
    HttpContext *ctx = new HttpContext();
    MHD_get_connection_values(connection, MHD_HEADER_KIND,
                              &HttpApplication::request_header_iterator,
                              (void *)ctx);
    MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND,
                              &HttpApplication::request_query_iterator,
                              (void *)ctx);
    ctx->request->method = method;
    ctx->request->url = url;

    bool isResolved = false;
    for (auto &module : app->modules) {
      isResolved = module->resolve(ctx);
      if (isResolved) break;
    }
    if (!isResolved) {
      ctx->response->status = 404;
      ctx->response->body = "Not Found";
    }
    struct MHD_Response *response = MHD_create_response_from_buffer(
        ctx->response->body.length(), ctx->response->body.data(),
        MHD_RESPMEM_PERSISTENT);
    for (auto &header : ctx->response->headers) {
      MHD_add_response_header(response, header.first.c_str(),
                              header.second.c_str());
    }
    int ret = MHD_queue_response(connection, ctx->response->status, response);
    MHD_destroy_response(response);
    delete ctx;
    return MHD_Result::MHD_YES;
  }

  static void handleSignal(int s) {
    printf("Caught signal %d\n", s);
    exit(1);
  }

 public:
  vector<HttpModule *> modules;
  vector<HttpActionFilter *> filters;

  void listen(unsigned short port) {
    using namespace std::placeholders;
    this->daemon =
        MHD_start_daemon(MHD_FLAG::MHD_USE_THREAD_PER_CONNECTION, port, NULL,
                         NULL, &HttpApplication::daemon_handler, (void *)this);

    // wait and handle SIGINT
    struct sigaction signalHandler;

    signalHandler.sa_handler = HttpApplication::handleSignal;
    sigemptyset(&signalHandler.sa_mask);
    signalHandler.sa_flags = 0;
    sigaction(SIGINT, &signalHandler, NULL);
    pause();
  }

  void registerModule(HttpModule *module) { modules.push_back(module); }
};

#define BUILD_HTTP_ROUTE(method, pattern, controller, handler) \
  HttpRoute(method, pattern, (HttpController *)controller,     \
            std::bind(handler, this, std::placeholders::_1))
}  // namespace Jaura

#endif