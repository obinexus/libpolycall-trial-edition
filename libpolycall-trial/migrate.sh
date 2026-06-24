# #!/bin/bash
# # Migration script for libpolycall restructuring

# # Exit on any error
# set -e

# echo "Starting libpolycall directory restructuring..."

# # Create new directory structure
# mkdir -p include/{cli,core,micro,network,parser,protocol,state}
# mkdir -p src/{cli,core,micro,network,parser,protocol,state}
# mkdir -p test/{cli,core,micro,network,parser,protocol,state}

# # Core module files
# echo "Moving core module files..."
# cp include/polycall.h include/core/
# mv src/polycall.c src/core/

# # Micro module files
# echo "Moving micro module files..."
# cp include/polycall_micro.h include/micro/
# mv src/polycall_micro.c src/micro/

# # Network module files
# echo "Moving network module files..."
# cp include/network.h include/network/
# mv src/network.c src/network/

# # Parser module files
# echo "Moving parser module files..."
# cp include/polycall_parser.h include/parser/
# cp include/polycall_token.h include/parser/
# cp include/polycall_tokenizer.h include/parser/
# mv src/polycall_parser.c src/parser/
# mv src/polycall_token.c src/parser/
# mv src/polycall_tokenizer.c src/parser/

# # Protocol module files
# echo "Moving protocol module files..."
# cp include/polycall_protocol.h include/protocol/
# mv src/polycall_protocol.c src/protocol/

# # State module files
# echo "Moving state module files..."
# cp include/polycall_state_machine.h include/state/
# mv src/polycall_state_machine.c src/state/

# # CLI module (create new files)
# echo "Creating CLI module files..."
# # Create src/cli/polycall_cli.c and src/cli/main.c from existing main.c
# mv main.c src/cli/main.c

# # Update include paths in all source files
# echo "Updating include paths in source files..."
# find src -name "*.c" -exec sed -i 's/#include "polycall\.h"/#include "core\/polycall.h"/g' {} \;
# find src -name "*.c" -exec sed -i 's/#include "polycall_micro\.h"/#include "micro\/polycall_micro.h"/g' {} \;
# find src -name "*.c" -exec sed -i 's/#include "network\.h"/#include "network\/network.h"/g' {} \;
# find src -name "*.c" -exec sed -i 's/#include "polycall_parser\.h"/#include "parser\/polycall_parser.h"/g' {} \;
# find src -name "*.c" -exec sed -i 's/#include "polycall_token\.h"/#include "parser\/polycall_token.h"/g' {} \;
# find src -name "*.c" -exec sed -i 's/#include "polycall_tokenizer\.h"/#include "parser\/polycall_tokenizer.h"/g' {} \;
# find src -name "*.c" -exec sed -i 's/#include "polycall_protocol\.h"/#include "protocol\/polycall_protocol.h"/g' {} \;
# find src -name "*.c" -exec sed -i 's/#include "polycall_state_machine\.h"/#include "state\/polycall_state_machine.h"/g' {} \;

# # Update include paths in header files
# find include -name "*.h" -exec sed -i 's/#include "polycall\.h"/#include "core\/polycall.h"/g' {} \;
# find include -name "*.h" -exec sed -i 's/#include "polycall_micro\.h"/#include "micro\/polycall_micro.h"/g' {} \;
# find include -name "*.h" -exec sed -i 's/#include "network\.h"/#include "network\/network.h"/g' {} \;
# find include -name "*.h" -exec sed -i 's/#include "polycall_parser\.h"/#include "parser\/polycall_parser.h"/g' {} \;
# find include -name "*.h" -exec sed -i 's/#include "polycall_token\.h"/#include "parser\/polycall_token.h"/g' {} \;
# find include -name "*.h" -exec sed -i 's/#include "polycall_tokenizer\.h"/#include "parser\/polycall_tokenizer.h"/g' {} \;
# find include -name "*.h" -exec sed -i 's/#include "polycall_protocol\.h"/#include "protocol\/polycall_protocol.h"/g' {} \;
# find include -name "*.h" -exec sed -i 's/#include "polycall_state_machine\.h"/#include "state\/polycall_state_machine.h"/g' {} \;

# # Copy new Makefile and CMakeLists.txt
# echo "Updating build files..."
# cp Makefile Makefile.old
# cp CMakeLists.txt CMakeLists.txt.old

# echo "Directory restructuring complete."
# echo "You may need to adjust include paths in some files manually if the automatic replacements missed any references."
# echo "Please verify the build with 'make clean && make' before committing changes."
