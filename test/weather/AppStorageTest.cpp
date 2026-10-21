#include <HalStorage.h>

#include <cassert>
#include <iostream>

#include "apps/AppStorage.h"
FakeStorage testStorage;
bool valid(JsonVariantConst doc) { return (doc["schema"] | 0) == 1 && doc["value"].is<int>(); }
int main() {
  using namespace appStorage;
  assert(validId("counter") && validId("my-app_2"));
  assert(!validId("") && !validId("../reader") && !validId("foo/bar") && !validId("a.b"));
  JsonDocument first, second, out;
  first["schema"] = 1;
  first["value"] = 5;
  second["schema"] = 1;
  second["value"] = 8;
  assert(save("counter", "state", first));
  assert(load("counter", "state", out, valid) && out["value"] == 5);
  assert(save("counter", "state", second));
  assert(load("counter", "state", out, valid) && out["value"] == 8);
  // Power cut after rotating current -> backup but before installing temp.
  assert(save("counter", "state", first));
  testStorage.renameCalls = 0;
  testStorage.failRename = 2;
  assert(!save("counter", "state", second));
  assert(load("counter", "state", out, valid) && out["value"] == 5);
  testStorage.failRename = 0;
  // Restore a valid primary, then corrupt its JSON and its semantic schema.
  assert(save("counter", "state", first));
  assert(save("counter", "state", second));
  const std::string path = "/.crosspoint/apps/counter/state.json";
  testStorage.files[path] = "{broken";
  assert(load("counter", "state", out, valid) && out["value"] == 5);
  testStorage.files[path] = "{\"schema\":2,\"value\":123}";
  assert(load("counter", "state", out, valid) && out["value"] == 5);
  testStorage.files[path] = std::string(8193, ' ');
  assert(load("counter", "state", out, valid) && out["value"] == 5);
  assert(!save("../reader", "state", first));
  assert(!load("counter", "../state", out));
  assert(save("device-info", "state", second));
  assert(load("counter", "state", out, valid) && out["value"] == 5);
  std::cout << "Application namespaces, atomic JSON rotation and interrupted/corrupt cache recovery passed\n";
}
