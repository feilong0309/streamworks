set_conf test/lsbench.properties search_offset=20
set_conf test/lsbench.properties decompose_paths=0,num_edges_to_process_M=1,query_algo=SingleLazy
run_test_lazy.x test/lsbench.properties test/synthetic/queries/path4/query100.list
run_test_lazy.x test/lsbench.properties test/synthetic/queries/path5/query100.list
set_conf test/lsbench.properties decompose_paths=1,num_edges_to_process_M=1,query_algo=PathLazy
run_test_lazy.x test/lsbench.properties test/synthetic/queries/path4/query100.list
run_test_lazy.x test/lsbench.properties test/synthetic/queries/path5/query100.list
set_conf test/lsbench.properties decompose_paths=0,num_edges_to_process_M=1,query_algo=Single
run_test_all.x test/lsbench.properties test/synthetic/queries/path4/query100.list
run_test_all.x test/lsbench.properties test/synthetic/queries/path5/query100.list
set_conf test/lsbench.properties decompose_paths=1,num_edges_to_process_M=1,query_algo=Path
run_test_all.x test/lsbench.properties test/synthetic/queries/path4/query100.list
run_test_all.x test/lsbench.properties test/synthetic/queries/path5/query100.list
