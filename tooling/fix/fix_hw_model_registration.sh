#!/bin/bash
# Register unregistered test_hw_model_*.c files in their CMakeLists.txt
cd "$(dirname "$0")/../.." || exit 1

count=0
for hwfile in $(find wheels/drivers/ -name "test_hw_model_*.c" 2>/dev/null); do
    dir=$(dirname "$hwfile")
    cmake="$dir/CMakeLists.txt"
    name=$(basename "$hwfile")
    name=${name#test_hw_model_}
    name=${name%.c}

    if grep -q "hw_model" "$cmake" 2>/dev/null; then continue; fi

    cat >> "$cmake" << BLOCK

add_executable(test_hw_model_${name}
    test_hw_model_${name}.c
    ../${name}.c
)
target_include_directories(test_hw_model_${name} PRIVATE
    \${CMAKE_CURRENT_SOURCE_DIR}/..
    \${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_${name} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_${name} PRIVATE m)

add_test(NAME ${name}_hw_model_tests COMMAND test_hw_model_${name})
BLOCK
    echo "ADDED $name"
    count=$((count + 1))
done
echo "Total: $count wheels patched"
