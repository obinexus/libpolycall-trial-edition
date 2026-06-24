// src/utils/functional/pipe.js
/**
 * Pipes functions from left to right, where output of each function is input to next
 * Pure point-free composition utility
 * @param {...Function} fns - Functions to compose
 * @returns {Function} Composed function
 */
const pipe = (...fns) => {
    if (fns.length === 0) return (x) => x;
    if (fns.length === 1) return fns[0];
    
    return fns.reduce((prevFn, nextFn) => 
        (...args) => nextFn(prevFn(...args))
    );
};

module.exports = pipe;