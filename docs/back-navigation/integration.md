# 合入桌面与游戏版本

## 补丁底座

`back-navigation.patch` 基于完整天气 0.2.0 扩展；先核对 CrossPoint HEAD、SDK SHA 和天气扩展补丁哈希。`full-extension.patch` 则可应用在干净 CrossPoint HEAD 上，一次带入天气 0.2.0 与本次返回修复。两者不要重复应用。补丁均未包含提交历史修改、SDK 修改或生成的 I18n/HTML 文件。

在独立集成副本中先 `git apply --check`，再应用所选补丁。源目录已经包含修复，不需要在交付源目录重打补丁。默认目标仅 `x4pro-gh_release`；版本字符串应由最终集成方统一，避免桌面/游戏固件仍显示同名候选版本。

## 最易冲突的共享文件

- `MappedInputManager.h/.cpp`：新增固定大小接触状态及导航抑制。保留既有 Home/电源双击/翻页映射；不要为了“单次消费”把查询函数改成读一次就清零。
- `ActivityManager.cpp`：render 前 beginFrame，render 返回后 publish；push/pop/replace 调度即 suppressNavigationInput；有 pending action 时跳过旧页面 loop。USB exclusive-storage 分支必须继续领先于导航栈处理。
- `HeaderBackTapTarget.h`：`set(Rect)` 只写 staging；`hide()` 清当前帧 staging；`clear()` 清已发布目标，用于切页。若新桌面直接调用 `set(x,y,w,h)`，须迁移为 geometry + set(Rect)。不要在 loop 中写 staging，也不要绕过 ActivityManager 在另一渲染任务发布。
- `BaseTheme.cpp`、书库/OPDS buildHeader：从实际 header band、status strip、leadingSize/actionOffsetY 算目标。书库/OPDS 的 Back 不再注册 FUI action 回调；右侧搜索/刷新仍由 FUI 处理。新增主题应继续调用这条路径，不要自己猜坐标。
- `OptionPopup.h`：先处理 Back，再处理 raw touch。用于游戏暂停/退出确认时，返回只关闭最上层；确认才退出/保存。
- `SettingsActivity`、`LibraryListActivity`：屏幕返回跳过纯标签焦点步骤，实体键原策略保留。
- `WeatherActivity/WeatherView/WeatherTouchLayout`：只共享底部按钮几何；不要覆盖天气请求、frameGate、深睡调度和低电量规则。

## 新桌面与原生游戏的约定

首页应像原 HomeActivity 一样传 `backButton=false`。原生应用优先继承 NativeAppActivity/UiListActivity，使用既有 onBackButton/onExit 保存和释放状态。游戏自绘页面若使用标题 Back，应经过 GUI.drawHeader 和 ActivityManager render；全屏无标题帧发布空目标，避免遗留桌面箭头。

输入轮询仍由主循环统一更新，不要让每个控件或渲染任务调用 mappedInput.update，也不要启用 SDK 异步输入轮询再同时调用同步 update。返回通过 finish / push / replace 调度；不可直接删除当前 Activity 或在渲染回调里切页。暂停、对话框、游戏界面切换若不是 Activity 栈切换，需自行设置清晰的输入优先级，并在改变交互含义时取消当前接触，防止抬起变成新页面按钮。

本次没有读取或移入两个并行会话的未完成代码，也没有向其发消息。集成方应将他们完成后的代码与本补丁合并，检查上述冲突，再运行测试与固件构建。

## 必跑验证

```sh
python3 scripts/test_back_navigation.py
python3 scripts/test_weather.py
python3 scripts/test_txt_paging.py
python3 test/test_list_viewport.py
cmake -S test -B build/host-test
cmake --build build/host-test -j 6
ctest --test-dir build/host-test --output-on-failure
pio run -e x4pro-gh_release
```

额外跑 SDK 的 `freeink-sdk/libs/ui/FreeInkUI/test/host/run.sh`。格式仅用 `./bin/clang-format-fix -g`。确保应用分区 0x640000、最终 bin 小于分区、ESP32-S3/X4 Pro board tag、版本与 SHA256 清单正确。最后按实机步骤验证并行新增桌面/游戏下的返回、暂停、退出确认、睡眠与原阅读恢复。
