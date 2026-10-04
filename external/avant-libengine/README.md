# avant-libengine

**状态：暂时停用。** 本目录下所有源码均已被注释，且未接入主 CMake 构建。以下内容描述该库的设计用途、用法与设计动机，供后续恢复或参考使用。

libengine 是一个**基于动态插件的工作流管线（pipeline）引擎**：把工作逻辑拆成若干可独立编译成 `.so` 的插件，按 XML 配置编排成管线，运行时动态加载执行，无需重新编译主程序即可增删、开关处理逻辑。

## 有什么用

它提供四个协作的概念：

| 组件 | 文件 | 职责 |
| --- | --- | --- |
| `context` | `context.h` | 管线中的数据载体。按类型分桶的 key-value 存储（bool/char/short/int/long/double/string/object*），插件间通过它传递数据。提供 `set<T>()` / `get<T>()` / `ref<T>()` / `clear()` |
| `plugin` | `plugin.h` / `plugin.cpp` | 处理单元基类。含名称、on/off 开关和一个纯虚函数 `bool run(context &ctx)`，返回 `false` 可中断整条管线 |
| `work` | `work.h` / `work.cpp` | 一条管线。按 `append` 的顺序依次执行其中的插件（每个插件可单独开关），销毁时负责释放各插件 |
| `workflow` | `workflow.h` / `workflow.cpp` | 顶层入口（单例）。解析 `workflow.xml`，把 work/plugin 定义实例化为 `work` 对象；对外暴露 `load(path)` 与 `run(work, input, output)` |
| `plugin_loader` | `plugin_loader.h` / `plugin_loader.cpp` | 通过 `dlopen` / `dlsym` / `dlclose` 从 `<root>/plugin/` 加载插件 `.so` 并缓存句柄 |

数据流：客户端 `input` → 写入 `context` → 管线中插件依次读取/改写 `context` → 最后从 `context` 取出 `output` 返回客户端。

## 怎么用

### 1. 编写一个插件

插件继承 `avant::engine::plugin` 实现 `run(context&)`，并暴露两个 C 导出函数 `create()` / `destory(plugin*)` 供 `dlsym` 获取：

```cpp
// echo_plugin.h
#pragma once
#include "engine/context.h"
#include "engine/plugin.h"

namespace avant
{
    namespace plugin
    {
        class echo_plugin : public avant::engine::plugin
        {
        public:
            echo_plugin();
            virtual ~echo_plugin();
            virtual bool run(avant::engine::context &ctx);
        };
    }
}

extern "C"
{
    avant::engine::plugin *create();
    void destory(avant::engine::plugin *p);
}
```

```cpp
// echo_plugin.cpp
#include <string>
#include "plugin/echo_plugin.h"

using namespace std;
using namespace avant::engine;
using namespace avant::plugin;

echo_plugin::echo_plugin() : plugin() {}
echo_plugin::~echo_plugin() {}

bool echo_plugin::run(context &ctx)
{
    std::string &input = ctx.ref<std::string>("input");
    ctx.ref<std::string>("output") = input + " is echo plugin run!";
    return true;
}

avant::engine::plugin *create()
{
    return new echo_plugin;
}

void destory(avant::engine::plugin *p)
{
    delete p;
    p = nullptr;
}
```

编译为共享库，输出到 `bin/plugin/` 目录（`plugin_loader` 固定从 `<root>/plugin/` 加载）：

```cmake
# CMakeLists.txt
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/plugin/)
add_library(echoplugin SHARED ${PROJECT_SOURCE_DIR}/src/plugin/echo_plugin.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/plugin.cpp)
```

### 2. 编写 workflow.xml

每个 `<work>` 对应一条管线，`switch` 为 `on`/`off`；内部按顺序排列 `<plugin>`，同样支持 `switch` 单独开关：

```xml
<workflow>
    <work name="echo_work" switch="on">
        <plugin name="echoplugin" switch="on"/>
        <plugin name="anotherplugin" switch="off"/>
    </work>
</workflow>
```

### 3. 初始化与运行

```cpp
// 启动时加载配置（workflow 是单例）
engine::workflow *work = avant::utility::singleton<engine::workflow>::instance();
work->load(get_root_path() + "/config/workflow.xml");

// 处理请求：按名字找到 work，执行管线
std::string output;
work->run("echo_work", "hello", output);
// output == "hello is echo plugin run!"
```

## 为什么要用

- **逻辑外置、动态加载**：业务逻辑编译为独立 `.so`，`dlopen` 按需加载，主程序不必为每个功能重新编译；
- **配置驱动**：管线结构、插件顺序、每个环节的 on/off 全部在 XML 里声明，改配置即可调整处理流程；
- **管线组合**：把大流程拆成小而独立的插件，同一插件可被不同 work 复用，插件返回 `false` 即可短路整条管线；
- **数据解耦**：插件之间只通过 `context` 传递命名数据，互不依赖具体实现，便于单独测试和替换。

## 当前状态

源码全部注释停用，且 `workflow.cpp` 依赖 `avant-xml`、`avant-log` 及 `utility/singleton.h`（主工程 `src/` 下的工具），恢复启用时需先取消注释并确认这些依赖可用，再自行接入 CMake。
