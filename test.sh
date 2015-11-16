# 2 cores ->
# 1 core <-
# l2fwd  on all cores

./bin/bridge -c 7 -n 3 -m 128 -- -p 3 --config-master config/config_test --config-fdir config/config.yaml
