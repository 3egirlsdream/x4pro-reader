# X4 Pro 桌面候选版 0.3.0

版本为 `1.6.5-desktop.0.3.0`。本目录保留原独立候选的开发说明，当前统一源码和交付状态见根 README。基线为 CrossPoint `93e98bb78702e29868a16a13b80c40e6b36ccdff` 加天气 0.2.0 扩展；FreeInk SDK 固定 `111fdcc7f0176c3ee38391a160ee296bf492dbd8`。README-X4Pro.md 记录天气扩展的历史基线，不代表本桌面候选版已安装。

新增首页为每页六本的书架及底部四列两行应用。全部已索引书籍可分页浏览；CLIX 仍最多 4,096 本、五层目录并保留原路径长度限制。具体操作、已验证范围和未实测限制见交付包的 `README-桌面候选版.md`、`reports/性能与测试报告.md` 和 `游戏合入说明.md`。

构建：`pio run -e x4pro-gh_release`。只分发 `.pio/build/x4pro-gh_release/firmware.bin` application 镜像，不把 factory 合并文件用于当前设备。构建不需要修改原天气仓库。完整源码压缩包包含固定 SDK 和图标文件；通过 Git 重建时应另核对 manifest 中的两个子模块修订。

验证：`cmake -S test -B build/host-test`、`cmake --build build/host-test -j 6`、`ctest --test-dir build/host-test --output-on-failure`。运行 `scripts/test_weather.py`、`scripts/test_txt_paging.py` 和 `scripts/check_chinese.py`；绘图与命中验证运行 `scripts/preview_desktop.py --output build/desktop-preview-images --font <X4ProSansSC-Regular.ttf 路径>`。格式化仅用仓库 `./bin/clang-format-fix -g`。

本会话未连接设备、未刷机，主机结果不是 ESP32 或墨水屏耗时。现有封面转换器不能细粒度取消，切页／退出可能等待一本转换完成。时间状态沿用 HAL RTC，不新增常开联网。默认中文及原阅读／天气／低电量策略保留；天气 0.2.0 的长篇／刷新／息屏实机复测不在本版补写通过结论。
