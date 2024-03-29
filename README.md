# jaura
Fast, opinionated, minimalist framework for building server-side applications on top of C++

## Overview
Jaura is a very simple C++ server-side application framework built on top of libhttpd. Jaura implementation is header-only in single header file that simplifies integration with any project. Inspired by NestJS and ASP.NET, but with simpler, easier and comprehensive API.

## Features
- [x] Flexible and easy-to-use API
- [x] Single thread per request
- [x] Cross-platform
- [x] RESTful oriented interface
- [ ] Support for middleware/action filter
- [ ] Support for dependency injection
- [ ] Support for authentication
- [ ] Support for easy database integration

## Dependencies
Jaura requires C++ 11. It does not have any platform specific dependency, but depends on **libmicrohttpd** library.

## Getting Started
This basic example demonstrating how to handle request using specific routes:

```cpp

#include "jaura.hpp"

using namespace Jaura;

class AppController : public HttpController {
 public:
  AppController() {
    HttpController();
    this->registerRoute(BUILD_HTTP_ROUTE("GET", "/", this, &AppController::root));
    this->registerRoute(BUILD_HTTP_ROUTE("GET", "/hello/:name", this, &AppController::hello));
    this->registerRoute(BUILD_HTTP_ROUTE("GET", "/multiply/:x([0-9]+)/with/:y([0-9]+)", this, &AppController::multiply));
    this->registerRoute(BUILD_HTTP_ROUTE("GET", "/foo", this, &AppController::foo));
  }

  void root(HttpContext *context) {
    context->response->status = 200;
    context->response->body = "Root";
  }

  void hello(HttpContext *context) {
    context->response->status = 200;
    context->response->body = "Hello " + context->request->params["name"] + "!";
  }

  void multiply(HttpContext *context) {
    string x = context->request->params["x"];
    string y = context->request->params["y"];
    int xnum = stoi(x);
    int ynum = stoi(y);
    string result = to_string(xnum * ynum);
    context->response->status = 200;
    context->response->body = "Result is " + result + "!";
  }

  void foo(HttpContext *context) {
    string bar = context->request->query["bar"];
    context->response->status = 200;
    context->response->body = "Bar is " + bar + "!";
  }
};

class AppModule : public HttpModule {
  public:
  AppModule(){
    HttpModule();
  }
};

int main() {
  AppController *appController = new AppController();
  AppModule *appModule = new AppModule();
  appModule->registerController((HttpController*) appController);
  HttpApplication *app = new HttpApplication();
  app->registerModule((HttpModule *)appModule);
  app->listen(8000);
}
```

