// src/utils/functional/curry.js
/**
 * Creates a curried version of a function
 * Pure function that enables partial application
 * @param {Function} fn - Function to curry
 * @returns {Function} Curried function
 */
const curry = (fn) => {
    const arity = fn.length;
    
    return function curried(...args) {
        if (args.length >= arity) {
            return fn(...args);
        }
        
        return function(...moreArgs) {
            return curried(...args, ...moreArgs);
        };
    };
};

module.exports = curry;