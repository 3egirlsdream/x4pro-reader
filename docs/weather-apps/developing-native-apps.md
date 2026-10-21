# 开发自己的前台应用

首版支持 C++ 应用随固件一起编译。入口为“首页 → 应用”。
现有天气、计数器、设备信息都从同一注册表创建。

## 最短开发流程

1. 将 `examples/native-apps/HelloToolActivity.h` 复制到 `src/apps/`。
2. 在 `lib/I18n/translations/english.yaml` 添加：

   ```yaml
   STR_APP_HELLO_TOOL: "Hello tool"
   ```

   在 `chinese-simplified.yaml` 添加同名中文翻译。其它语言会回退英文。
3. 在 `src/apps/AppRegistry.cpp` 加入头文件及注册项：

   ```cpp
   #include "HelloToolActivity.h"
   // 放进 APPS 数组：
   {"hello-tool", StrId::STR_APP_HELLO_TOOL, UIIcon::Blocks, create<HelloToolActivity>},
   ```

4. 运行 `pio run -e x4pro-gh_release`。构建同时生成翻译。
5. 通过已经在本机验证的更新入口安装新的 `firmware.bin`。
   这不是一个可单独复制到 XTOS 的应用包。

## 生命周期与可用接口

`NativeAppActivity` 继承底座 `UiListActivity`：

- `onEnter()`：加载状态，初始化静态/成员行缓冲，然后调用父类。
- `loop()`：默认已处理按键、触摸和滑动。额外处理必须保持每次循环有界。
- `activateIndex()`：处理统一的按键确认/触摸选中事件。
- `buildScreen()`：默认使用底座主题、中文字体回退和列表组件；可重写构建自己的界面。
- `render(RenderLock&&)`：需要自定义绘制时沿用现有渲染锁协议，使用 renderer 和 GUI。
- `onExit()`：保存改变的状态并释放资源，最后调用父类。系统进入天气休眠时也会退出当前应用。

`rows` 是 12 行的成员缓冲，`rowCount` 指定使用数量；每行必须设置唯一
`actionValue`。长列表应像 `CityPicker` 一样使用行提供器，不无限分配行对象。
当前“全部应用”菜单已用动态行提供器，显示所有注册应用及六个系统入口，
不再有旧的 11 个应用上限；模板自身的 12 行缓冲仍适用。
首页固定两行八格，其中四格为系统入口，余下四格按注册顺序显示原生应用。
新应用超出首页容量时仍能在“全部应用”访问。

`loadData(doc)` 和 `saveData(doc)` 读写
`/.crosspoint/apps/<id>/state.json`，JSON 上限 8 KiB。
ID 允许字母、数字、连字符、下划线，最长 40 字符，不允许路径分隔符。
请加入 schema 版本，状态变化时才保存。计数器是完整的持久化示例；
设备信息是第二个独立注册示例。

## 使用约束

- 应用只在前台运行；天气后台更新由系统管理。退出后不保留任务。
- 返回进入应用菜单；Home 进入首页。应用休眠后按电源键返回首页，不自动重开应用。
- 首版不提供通用后台定时、通用联网、动态加载二进制或脚本运行时。
- 原生 C++ 共享固件内存，不是安全沙箱；错误指针可以导致整机重启。
- 不直接操作 GPIO、电源或 SD SDK；使用 HAL，防止破坏屏幕及存储状态。
- 不在渲染路径联网或等待；不使用无限循环或连续刷新。尺寸从 renderer 读取。
- 新对象使用 `makeUniqueNoThrow` 并检查失败；大缓冲放成员或受控堆内存。
- 字体通过 SD 卡加载。随附 Noto CJK 字体及首次启动设置支持中文城市和界面。

## 验证

构建目标必须为 X4 Pro。运行 `python3 scripts/test_weather.py`，再在恢复条件
满足的样机上验证：应用发现 → 两种输入 → 保存退出 → 重开恢复 → 休眠 →
天气更新 → 唤醒首页 → 再次打开应用。连续进入退出 50 次，检查可用堆和最大块
没有持续下降，并确认阅读书签、进度及天气定时流程正常。

## 自定义绘制和输入

`renderer.drawText(UI_10_FONT_ID, x, y, utf8Text)` 支持 UTF-8；中文依赖已选择的
SD 字体回退。图形使用 `drawLine` / `drawRect` / `fillRect` 等，参见 WeatherView。
可在 `loop()` 中使用 `mappedInput.wasPressed/wasReleased(Button::...)` 与
`wasScreenTapped(x, y)`；默认 UiListActivity 已将这些事件转换为行激活。
成员状态与渲染任务共享时按底座规则持有 `RenderLock`，重绘使用 `requestUpdate()`，
不要自建持续刷屏循环。完整自绘 Activity 的生命周期可参考 WeatherActivity，
自开发工具仍不得接管它的后台联网或电源调度。
