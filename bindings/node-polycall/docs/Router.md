# Router

The Router module provides a flexible routing system for handling HTTP-like requests in the PolyCall library. It extends EventEmitter to provide event-based functionality.

## Installation

The Router class is included in the PolyCall library:

```js
const { Router } = require('node-polycall');
```

## Usage

```js
const router = new Router();

// Add a route with method-specific handlers
router.addRoute('/books', {
    GET: async (ctx) => {
        // Handle GET request
        return { success: true, data: books };
    },
    POST: async (ctx) => {
        // Handle POST request 
        return { success: true, data: newBook };
    }
});

// Handle an incoming request
await router.handleRequest('/books', 'GET');
```

## API Reference

### Constructor

```js
constructor()
```

Creates a new Router instance with an empty routes map.

### Methods

#### addRoute(path, handlers)

Registers a new route with method-specific handlers.

- `path` - The URL path for the route
- `handlers` - Either a function or an object with method-specific handlers

```js
// Function handler
router.addRoute('/status', (ctx) => {
    return { status: 'ok' };
});

// Method-specific handlers
router.addRoute('/users', {
    GET: (ctx) => {},
    POST: (ctx) => {},
    DELETE: (ctx) => {}
});
```

#### handleRequest(path, method, data)

Processes an incoming request.

- `path` - Request URL path
- `method` - HTTP method (GET, POST, etc)
- `data` - Optional request payload
- Returns: Promise that resolves with handler response

```js
const result = await router.handleRequest('/books', 'GET', {});
```

#### findRoute(path)

Finds a registered route matching the given path.

- `path` - URL path to match
- Returns: Route object if found, null otherwise

#### parseQueryString(path) 

Parses query parameters from a URL path.

- `path` - URL path with query string
- Returns: Object with parsed parameters

#### normalizePath(path)

Normalizes a URL path by adding leading slash and removing trailing slash.

- `path` - URL path to normalize
- Returns: Normalized path string

#### printRoutes()

Prints all registered routes for debugging purposes.

## Events

The Router emits the following events:

- `error` - When an error occurs during request handling

## Examples

### Basic Routing

```js
const router = new Router();

// Add routes
router.addRoute('/', {
    GET: async () => ({ message: 'Welcome!' })
});

router.addRoute('/users', {
    GET: async () => ({ users: [] }),
    POST: async (ctx) => {
        const user = ctx.data;
        return { created: user };
    }
});

// Handle requests
await router.handleRequest('/', 'GET');
await router.handleRequest('/users', 'POST', { name: 'John' });
```

### Error Handling

```js
router.on('error', (error) => {
    console.error('Router error:', error);
});

router.addRoute('/protected', {
    GET: () => {
        throw new Error('Unauthorized');
    }
});
```

### Query Parameters

```js
router.addRoute('/search', {
    GET: (ctx) => {
        const { query } = ctx;
        return { query };
    }
});

// Request: /search?term=test
const result = await router.handleRequest('/search?term=test', 'GET');
// result.query = { term: 'test' }
```

## Best Practices

1. Always handle errors in route handlers
2. Use consistent path formats (with/without trailing slash)
3. Validate request data before processing
4. Use async handlers for asynchronous operations
5. Keep route handlers focused and modular

## See Also

- [PolyCallClient Documentation](./PolyCallClient.md)
- [State Documentation](./State.md)
- [NetworkEndpoint Documentation](./NetworkEndpoint.md)
