# Monitor Demo

# How to Run
安装好[cmake](https://cmake.org/download/)（可使用默认安装，选择把cmake自动加入到命令行）
cd build。然后运行以下命令
```
cmake .. -G "Visual Studio 17 2022" -A Win32
```
构建过程如下:
```
D:\srccode\windowsmonitor\monitordemo\build>cmake .. -G "Visual Studio 17 2022" -A Win32
CMake Deprecation Warning at CMakeLists.txt:2 (cmake_minimum_required):
  Compatibility with CMake < 2.8.12 will be removed from a future version of
  CMake.

  Update the VERSION argument <min> value or use a ...<max> suffix to tell
  CMake that the project does not need compatibility with older versions.


-- Selecting Windows SDK version 10.0.22621.0 to target Windows 10.0.19045.
-- The C compiler identification is MSVC 19.33.31630.0
-- The CXX compiler identification is MSVC 19.33.31630.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.33.31629/bin/Hostx64/x86/cl.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.33.31629/bin/Hostx64/x86/cl.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Configuring done
-- Generating done
-- Build files have been written to: D:/srccode/windowsmonitor/monitordemo/build

D:\srccode\windowsmonitor\monitordemo\build>
```
然后在build目录中打开生成的sln，把Demo项目设置为启动项，编译运行即可。


# 涉及知识点
## 函数调用约定
[调用约定](https://learn.microsoft.com/zh-cn/cpp/cpp/calling-conventions?view=msvc-170)
。其中[__stdcall](https://learn.microsoft.com/zh-cn/cpp/cpp/stdcall?view=msvc-170)可参见[https://learn.microsoft.com/zh-cn/cpp/cpp/stdcall?view=msvc-170](https://learn.microsoft.com/zh-cn/cpp/cpp/stdcall?view=msvc-170)

## 线程启动
[_beginthreadex](https://learn.microsoft.com/zh-cn/cpp/c-runtime-library/reference/beginthread-beginthreadex?view=msvc-170)
## 句柄复制
[DuplicateHandle](https://learn.microsoft.com/zh-cn/windows/win32/api/handleapi/nf-handleapi-duplicatehandle?redirectedfrom=MSDN)
## detours hook
可以参考[https://github.com/iherewaitfor/detoursdemo](https://github.com/iherewaitfor/detoursdemo)
## WinodwsHook
SetWindowsHookExA

CallNextHookEx

UnhookWindowsHookEx