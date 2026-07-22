# Hardware Book Editorial Style

Use these rules for reader-facing prose. They are editing rules, not manuscript content.

## Preserve the knowledge payload

- Improve wording by clarifying, reconnecting, or expanding. Do not remove mechanisms, numbers, equations, traces, cases, exercises, answers, warnings, or boundary conditions.
- Preserve distinctions that hardware correctness depends on: request versus acceptance versus completion versus persistence; execution versus architectural commit; logical address versus physical transaction; average behavior versus tail behavior.
- Keep concrete quantities and worked examples. Prefer correcting their setup or interpretation to replacing them with general claims.

## Build each explanation as a hardware path

- Name the actor and state before using pronouns such as “它” or “这一步”.
- Explain mechanisms in the order “input or precondition → data path → control decision → timing or commit boundary → visible result → failure and recovery”.
- Give each sentence one main causal claim. Split a sentence when two independent mechanisms, exceptions, or time scales compete for attention.
- Introduce a term as Chinese name plus English name or abbreviation at first use; then keep one spelling and capitalization.

## Keep navigation stable

- Refer to chapter and section titles or topics, not historical chapter numbers.
- Replace `§一`, “本章第五节”, “上一节”, and “下一节” with the target section title when restructuring could make the reference ambiguous.
- Use the current table-of-contents title when a unique chapter owns the topic. Use “前文的……章节/案例” only when the material is deliberately distributed across several current chapters.
- Do not expose migration language such as “原第几章”, “拆分后”, or “旧稿” to readers.

## Keep the Chinese technical prose direct

- Prefer a concrete subject and verb over stacked abstract nouns: “控制器写回完成项” is clearer than “完成信息的写回过程”.
- Use Chinese quotation marks for Chinese prose and reserve straight ASCII quotes for literal code. Put one half-width space between Chinese prose and adjacent Latin words, abbreviations, or Arabic numerals; do not insert spaces inside code literals or identifiers.
- Use parallel wording for parallel stages and table columns.
- Avoid filler conclusions such as “由此可见” when the next sentence can state the conclusion directly.
- Do not vary a technical term merely for literary effect. In particular, keep Cache/cache, TLB, MMIO, DMA, IOMMU, PCIe, NVMe, PHY, FTL, fence, trace, bit, byte, and cycle internally consistent with the chapter’s established convention.
- Keep warnings specific: state the violated condition, the observable symptom, and the measurement or correction that closes the loop.
- Prefer mechanism verbs over coercive or violent metaphors. Write “约束决定”“条件触发”“占用预算”“达到寿命上限”“暴露错误” and “传递数据”, not “逼出来”“吃掉预算”“擦穿”“逼出错误” or “喂给”。
- Avoid casual evaluation and debugging slang such as “挨打”“硬扛”“死磕”“翻车”“遮羞布”“兜底” and “白白”。Name the actual load, limit, failure, recovery mechanism, or unnecessary work instead.
- Use an explicit causal transition when moving from an observation to a design decision: state the measured condition or physical limit first, then state the resulting design choice. Do not use a vivid metaphor as a substitute for that transition.

## Comment pseudocode at hardware boundaries

- Comment the invariant or advancement rule, not a word-for-word translation of the next line.
- State where a result becomes visible: combinational settling, active clock edge, bus sampling point, completion entry, persistence point, or recovery checkpoint.
- For transactions, distinguish request, acceptance, data visibility, completion, and ownership return. For loops, name the exit and failure conditions. For state machines and microcode, distinguish same-cycle parallel controls from next-state transfer.
- Keep comments short enough that the algorithm remains scannable. Two focused header comments are the default; add inline comments only when a branch or exceptional path would otherwise be ambiguous.
