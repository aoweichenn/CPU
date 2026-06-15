# Books

这个目录是全仓库的书库根。每一本书都必须有自己的目录，不能把正文、实验、工具和结果直接散落在仓库根目录。

当前书籍：

| book id | status | description |
|---|---|---|
| `cpu-volume-1` | active | CPU 底层原理教材第一册：程序、C++、二进制、x86-64、汇编、Linux 工具链和性能测量。 |

## 书籍目录规范

```text
books/<book-id>/
  README.md
  Makefile
  source/
  materials/
  labs/
  reports/
  results/
  tools/
```

目录含义：

- `source/`：正式成书源码，例如 LaTeX、Markdown、脚本生成工程。
- `materials/`：草稿、素材、参考资料、课程计划、题单。
- `labs/`：和本书绑定的代码实验。
- `reports/`：报告模板、读书报告、实验报告。
- `results/`：实验结果和生成数据，通常只跟踪 `.gitkeep` 或精选样例。
- `tools/`：本书专用工具脚本。

跨书共享的脚本、模板或风格文件必须先证明有复用价值，再提升到仓库级目录，避免为了抽象而抽象。
