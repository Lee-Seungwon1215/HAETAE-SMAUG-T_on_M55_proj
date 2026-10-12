# STM32N657 publication evidence

Two campaigns form the public measurement evidence:

- `20260822-final-paper-4backend-02/`: end-to-end Intrinsic, Assembly, and
  Final/Slothy results, with the preserved C Reference as context. Four
  order-balanced sets provide 400 samples per configuration and result cell.
- `20260823-slothy-matched-per-operation-icache-board-01/`: exact matched
  pre-/post-Slothy kernel measurements. Two reversed-order sets provide 200
  samples per stage and result cell.

The compact Git version retains raw `output.log` files, campaign manifests,
size summaries needed by the analyzer, and generated statistics. Duplicate
ELF/map files, host/probe logs, source snapshots, and run descriptors containing
the local probe identifier are intentionally excluded.

Recompute both result sets from the repository root:

```sh
python3 scripts/analyze_final_paper_campaign.py \
  measurements/stm32n657/20260822-final-paper-4backend-02

python3 scripts/stm32n657_slothy_matched_campaign.py analyze \
  --output measurements/stm32n657/20260823-slothy-matched-per-operation-icache-board-01
```
