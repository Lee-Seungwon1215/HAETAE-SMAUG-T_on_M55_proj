# Matched pre-/post-Slothy STM32N657 results

Each row pools two reversed-order sets. `Speedup` is pre-Slothy cycles divided by post-Slothy cycles.

| Pair | Parameter | Operation | Pre cycles | Post cycles | Speedup | Reduction |
|---|---|---|---:|---:|---:|---:|
| keccak_x4 | haetae2 | keccak_permutation_x4_mve | 25,755.0 | 25,755.0 | 1.0000x | 0.00% |
| keccak_x4 | haetae2 | shake128_x4_mve | 26,592.0 | 26,589.0 | 1.0001x | 0.01% |
| keccak_x4 | haetae3 | keccak_permutation_x4_mve | 25,994.0 | 25,994.0 | 1.0000x | 0.00% |
| keccak_x4 | haetae3 | shake128_x4_mve | 26,737.0 | 26,784.0 | 0.9982x | -0.18% |
| keccak_x4 | haetae5 | keccak_permutation_x4_mve | 25,755.0 | 25,755.0 | 1.0000x | 0.00% |
| keccak_x4 | haetae5 | shake128_x4_mve | 26,656.0 | 26,653.0 | 1.0001x | 0.01% |
| keccak_x4 | smaugt1 | keccak_permutation_x4_mve | 25,754.0 | 25,754.0 | 1.0000x | 0.00% |
| keccak_x4 | smaugt1 | shake128_x4_mve | 26,587.0 | 26,582.0 | 1.0002x | 0.02% |
| keccak_x4 | smaugt3 | keccak_permutation_x4_mve | 25,898.0 | 25,898.0 | 1.0000x | 0.00% |
| keccak_x4 | smaugt3 | shake128_x4_mve | 26,825.0 | 26,822.0 | 1.0001x | 0.01% |
| keccak_x4 | smaugt5 | keccak_permutation_x4_mve | 25,898.0 | 25,898.0 | 1.0000x | 0.00% |
| keccak_x4 | smaugt5 | shake128_x4_mve | 26,826.0 | 26,823.0 | 1.0001x | 0.01% |
| haetae_forward_ntt | haetae2 | ntt | 3,974.0 | 2,590.0 | 1.5344x | 34.83% |
| haetae_forward_ntt | haetae3 | ntt | 4,080.0 | 2,569.0 | 1.5882x | 37.03% |
| haetae_forward_ntt | haetae5 | ntt | 4,067.0 | 2,806.0 | 1.4494x | 31.01% |
| haetae_inverse_ntt | haetae2 | invntt | 6,393.0 | 3,765.0 | 1.6980x | 41.11% |
| haetae_inverse_ntt | haetae3 | invntt | 5,338.0 | 3,679.0 | 1.4509x | 31.08% |
| haetae_inverse_ntt | haetae5 | invntt | 5,427.0 | 3,633.0 | 1.4938x | 33.06% |
| haetae_pointwise | haetae2 | pointwise | 1,846.5 | 1,657.0 | 1.1144x | 10.26% |
| haetae_pointwise | haetae3 | pointwise | 1,868.5 | 1,697.0 | 1.1011x | 9.18% |
| haetae_pointwise | haetae5 | pointwise | 1,852.0 | 1,662.0 | 1.1143x | 10.26% |
| haetae_pointwise_acc | haetae2 | pointwise_acc | 6,179.0 | 5,234.0 | 1.1806x | 15.29% |
| haetae_pointwise_acc | haetae2 | pointwise_acc_precentered | 4,415.0 | 3,645.0 | 1.2112x | 17.44% |
| haetae_pointwise_acc | haetae2 | pointwise_acc_u16_precentered | 4,466.0 | 3,474.0 | 1.2855x | 22.21% |
| haetae_pointwise_acc | haetae3 | pointwise_acc | 26,582.0 | 25,100.0 | 1.0590x | 5.58% |
| haetae_pointwise_acc | haetae3 | pointwise_acc_precentered | 6,242.0 | 5,155.0 | 1.2109x | 17.41% |
| haetae_pointwise_acc | haetae3 | pointwise_acc_u16_precentered | 6,194.0 | 4,754.0 | 1.3029x | 23.25% |
| haetae_pointwise_acc | haetae5 | pointwise_acc | 27,727.0 | 27,846.0 | 0.9957x | -0.43% |
| haetae_pointwise_acc | haetae5 | pointwise_acc_precentered | 7,041.0 | 5,759.0 | 1.2226x | 18.21% |
| haetae_pointwise_acc | haetae5 | pointwise_acc_u16_precentered | 7,059.0 | 5,459.5 | 1.2930x | 22.66% |
| haetae_barrett | haetae2 | barrett_tight | 618.0 | 495.0 | 1.2485x | 19.90% |
| haetae_barrett | haetae3 | barrett_tight | 618.0 | 398.0 | 1.5528x | 35.60% |
| haetae_barrett | haetae5 | barrett_tight | 618.0 | 398.0 | 1.5528x | 35.60% |
| haetae_centered | haetae2 | reduce2q | 1,200.0 | 928.5 | 1.2924x | 22.62% |
| haetae_centered | haetae2 | freeze2q | 1,392.0 | 1,044.0 | 1.3333x | 25.00% |
| haetae_centered | haetae2 | freeze | 1,455.0 | 1,044.0 | 1.3937x | 28.25% |
| haetae_centered | haetae3 | reduce2q | 1,200.0 | 883.0 | 1.3590x | 26.42% |
| haetae_centered | haetae3 | freeze2q | 1,392.0 | 1,044.0 | 1.3333x | 25.00% |
| haetae_centered | haetae3 | freeze | 1,455.0 | 1,044.0 | 1.3937x | 28.25% |
| haetae_centered | haetae5 | reduce2q | 1,200.0 | 883.0 | 1.3590x | 26.42% |
| haetae_centered | haetae5 | freeze2q | 1,392.0 | 1,111.0 | 1.2529x | 20.19% |
| haetae_centered | haetae5 | freeze | 1,455.0 | 1,044.0 | 1.3937x | 28.25% |
| haetae_fft | haetae2 | fft | 12,665.0 | 12,620.0 | 1.0036x | 0.36% |
| haetae_fft | haetae2 | fft_fused_sqabs | 13,540.0 | 12,472.0 | 1.0856x | 7.89% |
| haetae_fft | haetae3 | fft | 12,425.0 | 12,392.0 | 1.0027x | 0.27% |
| haetae_fft | haetae3 | fft_fused_sqabs | 13,529.0 | 12,218.0 | 1.1073x | 9.69% |
| haetae_fft | haetae5 | fft | 12,553.0 | 12,429.0 | 1.0100x | 0.99% |
| haetae_fft | haetae5 | fft_fused_sqabs | 13,505.0 | 12,382.0 | 1.0907x | 8.32% |
| haetae_sampler | haetae2 | sampler_cdt_leaf | 202.0 | 189.0 | 1.0688x | 6.44% |
| haetae_sampler | haetae2 | sampler_gauss_N | 611,472.0 | 608,051.0 | 1.0056x | 0.56% |
| haetae_sampler | haetae3 | sampler_cdt_leaf | 202.0 | 189.0 | 1.0688x | 6.44% |
| haetae_sampler | haetae3 | sampler_gauss_N | 611,937.0 | 608,941.0 | 1.0049x | 0.49% |
| haetae_sampler | haetae5 | sampler_cdt_leaf | 236.0 | 189.0 | 1.2487x | 19.92% |
| haetae_sampler | haetae5 | sampler_gauss_N | 611,666.0 | 608,175.0 | 1.0057x | 0.57% |
| smaugt_toom | smaugt1 | poly_mul_acc | 37,145.0 | 34,262.5 | 1.0841x | 7.76% |
| smaugt_toom | smaugt1 | matrix_vec_mult | 131,807.0 | 121,354.0 | 1.0861x | 7.93% |
| smaugt_toom | smaugt3 | poly_mul_acc | 36,928.0 | 34,158.0 | 1.0811x | 7.50% |
| smaugt_toom | smaugt3 | matrix_vec_mult | 284,493.0 | 261,491.0 | 1.0880x | 8.09% |
| smaugt_toom | smaugt5 | poly_mul_acc | 36,826.0 | 34,096.0 | 1.0801x | 7.41% |
| smaugt_toom | smaugt5 | matrix_vec_mult | 498,793.0 | 458,011.0 | 1.0890x | 8.18% |
