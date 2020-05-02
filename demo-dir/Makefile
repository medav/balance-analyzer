
demo-complex:
	clang-9 -S -emit-llvm ../test-programs/full/complex.c -I ../test-programs/include
	distiller complex.ll 4 complex.df
	balance-analyzer complex.df 4

demo-full2:
	clang-9 -S -emit-llvm ../test-programs/full/full2.c -I ../test-programs/include
	distiller full2.ll 4 full2.df
	balance-analyzer full2.df 4

demo-vulcan3:
	clang-9 -S -emit-llvm ../test-programs/smi-vulcan/vulcan3.c -I ../test-programs/include
	distiller vulcan3.ll 8 vulcan3.df
	balance-analyzer vulcan3.df 8

demo-bias-add:
	clang-9 -S -emit-llvm ../test-programs/smi-proximath/bias-add.c -I ../test-programs/include
	distiller bias-add.ll 3 bias-add.df
	balance-analyzer bias-add.df 3

clean:
	rm -rf *.ll *.df *.out *.dot *.png edits
