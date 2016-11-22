# Prerequisite

1. Download and install [LLVM-3.4](http://www.llvm.org) and the Clang frontend (version 3.4) following the instructions given in the [Getting Started](http://llvm.org/releases/3.4/docs/GettingStarted.html) manual.
  - Compile LLVM with the debug flag set (e.g. `./configure --disable-optimized` followed by `make -j4`)
2. Following the __Getting Started Quickly (A Summary)__ you should have a `build` directory in `/path_to_llvm_directory`.
3. Check, if the HelloWorld Example is working. (optionally)


# Build Instructions

1. Create a new directory in `/path_to_llvm_directory/build/lib/Transforms`.
2. Clone the code of DFGenTool into that directory.
3. Execute `make` in the newly created directory `DFGenTool`.
4. A shared object file named `loop_graph_analysis_0.so` will be created in `/path_to_llvm_directory/build/Debug+Asserts/lib`.

# Using DFGenTool

Examples are supplied in the `example` directory of the repository (currently residing in `/path_to_llvm_directory/build/lib/Transforms/DFGenTool/`). In this section we walk through the `simpleAdder`

1. Generate a file containing the intemediate code (`.ll`) execute:
   ```
   /path_to_llvm_directory/build/bin/clang -O1 -emit-llvm simpleAdder.c -S -o simpleAdder_generated.ll
   ```
2. To create a `dot` file from the intermediate code which can be further processed by graphviz tools, use the following command:
   ```
   /path_to_llvm_directory/build/bin/opt -load  /path_to_llvm_directory/built/Debug+Asserts/lib/loop_graph_analysis_0.so -loop-graph-analysis-0 simpleAdder_generated.ll
   ```
3. Two new files are created in the same directory: `0.loop_analysis_graph.dot` and `0.loop_analysis_graph.graph`. Compare both dot files containing the flow graphs.