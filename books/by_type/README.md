# Books By Type

这个目录按成书形态整理入口，不替代真实书籍目录。真实工程仍在
`books/<book-id>/` 下，构建、导出、测试和正文里的源码引用也继续使用真实路径。

使用这里的入口时，目标很简单：

- `theory/`：原理卷和主教材，负责讲清问题、概念、推导和设计取舍。
- `practice/`：实践卷，负责分阶段任务、实验步骤、验收标准和报告。
- `code/`：源码卷或代码卷，负责放完整工程清单、构建入口、测试、benchmark 和阅读路径。
- `planned/`：尚未开工或只做规划的新卷。

现有映射：

| type | entry | real book |
|---|---|---|
| theory | `theory/cpu_volume_1` | `../cpu-volume-1` |
| theory | `theory/cpu_volume_2` | `../cpu-volume-2` |
| theory | `theory/cpu_volume_3` | `../cpu-volume-3` |
| theory | `theory/cpp_zero_to_advanced` | `../cpp-zero-to-advanced` |
| theory | `theory/algorithm_interview` | `../algorithm-interview` |
| practice | `practice/cpu_volume_1_practice` | `../cpu-volume-1-practice` |
| practice | `practice/cpu_volume_3_practice` | `../cpu-volume-3-practice` |
| code | `code/compute_systems_engine_code` | `../compute-systems-engine-code` |
| code | `code/cpu_volume_3_source` | `../cpu-volume-3-source` |

新增一本书时，先在 `books/<book-id>/` 建真实目录，再按需要把入口登记到
`by_type/` 和 `by_topic/`。不要把正文、实验或构建产物直接放进分类目录。
