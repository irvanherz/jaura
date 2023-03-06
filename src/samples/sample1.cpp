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