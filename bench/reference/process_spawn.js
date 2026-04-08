const { execSync } = require('child_process');
const output = execSync('seq 1 10000', { encoding: 'utf8' });
console.log(output.length);
