// src/utils/functional/reduce.js
const curry = require('./curry');

/**
 * Reduces an array to a single value using an accumulator function
 * Pure point-free array reduction
 * @param {Function} fn - Reducer function
 * @param {*} initial - Initial accumulator value
 * @param {Array} arr - Array to reduce
 * @returns {*} Final accumulated value
 */
const reduce = curry((fn, initial, arr) => {
    if (!Array.isArray(arr)) {
        throw new TypeError('Third argument must be an array');
    }
    
    let acc = initial;
    for (let i = 0; i < arr.length; i++) {
        acc = fn(acc, arr[i], i, arr);
    }
    return acc;
});

module.exports = reduce;