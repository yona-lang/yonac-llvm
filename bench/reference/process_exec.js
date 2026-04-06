const { execSync } = require('child_process');
const a = execSync('echo hello').toString().trim();
const b = execSync('echo world').toString().trim();
const c = execSync('echo yona').toString().trim();
console.log(a.length + b.length + c.length);
