const fs = require("fs");
const child = require("child_process");
// get paths
const VMLOC = process.env.PWD + "/.bin/mvm";
const TLOC = process.env.PWD + "/tests/";
// get tests
let tests = fs.readdirSync(TLOC);
tests = tests.filter(x => x.endsWith(".magenta")) // make unique
	.map(x => x.split(".")[0]) // remove file extension
// run tests
let ranTests = 0;
let passedTests = [];
let failedTests = [];
tests.forEach(t => {
	let test = child.spawn(VMLOC, [TLOC + t + ".magenta"]);
	let output = "";
	test.stdout.on("data", data => output = output + data);
	test.on("close", () => {
		fs.readFile(TLOC + t + ".txt", "UTF-8", (e, data) => {
			if (data == output) {
				passedTests.push(t);
				return done();
			} else {
				if (e)
					console.error(e);
				failedTests.push([t, e || data, output]);
				return done();
			}
		});
	});
});
// print results
function done() {
	// if not all tests have been run yet
	ranTests++;
	if (ranTests != tests.length) return;
	// alphabetical sort
	passedTests = passedTests.sort((a, b) => a.localeCompare(b));
	failedTests = failedTests.sort((a, b) => a[0].localeCompare(b[0]));
	if (passedTests.length > 0) {
		console.log(`==> ${passedTests.length} successful tests:`);
		passedTests.forEach(t => console.log("\033[32;1m✓\033[m", t));
	}
	if (failedTests.length > 0) {
		console.log(`==> ${failedTests.length} failed tests:`);
		failedTests.forEach(t => {
			console.log("\033[31;1m✗\033[m", t[0]);
			console.log("  expected:", t[1]);
			console.log("  got:", t[2]);
		});
	}
}
