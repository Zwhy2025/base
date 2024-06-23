# 基线工作流
## 项目结构
```
.
├── C++/
│   ├── Doxyfile
│   ├── dock/
│   ├── src/
│   └── test/
├── Python/
│   ├── src/
│   └── test/
└── README.md
```
## 默认配置
编码格式：UTF-8 with BOM
VSCode : "cmake.generator": "Visual Studio 17 2022"
CLion  : 在输出控制台中模拟终端
VS2022 : .editorconfig
## 项目说明
## C++
### Doxygen文档生成
```
doxygen Doxyfile
cd dock/html
```
执行`python -m http.server `进入网页文档,或者直接打开index.html查看自动生成的文档
### Google Test单元测试
-
## Python

