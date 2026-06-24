# PolyCall Parser Documentation

## Overview

The PolyCall Parser is a high-performance AST (Abstract Syntax Tree) parser with support for point-free style operations, node transformations, and optimization passes. It implements a data-oriented design pattern focused on cache alignment and efficient memory usage.

## Core Components

### AST Node Types
```c
typedef enum {
    AST_NONE          = 0x00,
    AST_PROGRAM       = 0x01,
    AST_FUNCTION      = 0x02,
    AST_VARIABLE      = 0x04,
    AST_EXPRESSION    = 0x08,
    AST_STATEMENT     = 0x10,
    AST_BLOCK         = 0x20,
    AST_CONTROL_FLOW  = 0x40,
    AST_ERROR         = 0x80
} PolycallASTType;
```

### Node Structure
The parser uses a cache-aligned node structure:
```c
typedef struct PolycallASTNode {
    PolycallASTType type;           // 4 bytes
    PolycallASTAttributes attrs;    // 4 bytes
    PolycallValue value;            // 24 bytes
    uint32_t line;                  // 4 bytes
    uint32_t column;                // 4 bytes
    uint32_t child_count;           // 4 bytes
    struct PolycallASTNode** children; // 8 bytes
    struct PolycallASTNode* parent;    // 8 bytes
} PolycallASTNode;                    // Total: 60 bytes
```

## API Reference

### Parser Creation and Cleanup
```c
PolycallParser* polycall_parser_create(const PolycallParserConfig* config);
void polycall_parser_destroy(PolycallParser* parser);
```

### Parsing Operations
```c
PolycallAST* polycall_parser_parse_file(PolycallParser* parser, const char* filename);
PolycallAST* polycall_parser_parse_string(PolycallParser* parser, const char* input, size_t length);
```

### AST Manipulation
```c
PolycallASTNode* polycall_ast_create_node(PolycallASTType type, const PolycallValue* value);
bool polycall_ast_add_child(PolycallASTNode* parent, PolycallASTNode* child);
void polycall_ast_destroy_node(PolycallASTNode* node);
```

### Point-Free Operations
```c
PolycallASTNode* polycall_ast_map(const PolycallASTNode* node, ASTTransform transform);
PolycallAST* polycall_ast_filter(const PolycallAST* ast, ASTPredicate predicate);
void polycall_ast_visit(PolycallASTNode* node, ASTVisitor visitor, void* user_data);
```

## Error Handling

The parser provides detailed error information through:
```c
const char* polycall_parser_get_error(const PolycallParser* parser);
```

Error information includes:
- Error message
- Line number
- Column number
- Error context

## Performance Considerations

1. Memory Pool
   - Uses a pre-allocated node pool
   - Efficient node allocation/deallocation
   - Minimizes memory fragmentation

2. Cache Alignment
   - Node structure optimized for cache lines
   - Contiguous memory for child nodes
   - Flat node array for traversal

3. Optimization Passes
```c
PolycallAST* polycall_ast_optimize(const PolycallAST* ast, uint32_t level);
```
- Level 1: Basic optimizations (redundant node removal)
- Level 2: Intermediate optimizations (constant folding)  
- Level 3: Advanced optimizations (control flow)

## Thread Safety

The parser is not thread-safe by default. Users must implement synchronization when sharing parser instances across threads.

## Examples

### Basic Usage
```c
// Create parser
PolycallParser* parser = polycall_parser_create(NULL);

// Parse file
PolycallAST* ast = polycall_parser_parse_file(parser, "input.pc");

// Optimize AST
PolycallAST* optimized = polycall_ast_optimize(ast, 1);

// Cleanup
polycall_ast_destroy_node(ast->root);
polycall_parser_destroy(parser);
```

### Using Transformations 
```c
// Create transform chain
ASTTransform transforms[] = { transform1, transform2 };
PolycallASTTransforms* chain = polycall_ast_create_transforms(transforms, 2);

// Apply transforms
PolycallAST* transformed = polycall_ast_apply_transforms(ast, chain);

// Cleanup
polycall_ast_destroy_transforms(chain);
```