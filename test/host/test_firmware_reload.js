const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../../www/firmware.js'), 'utf8');

function page(version) {
    const navigations = [];
    const context = vm.createContext({
        URL,
        document: { querySelector: () => version ? { content: version } : null },
        window: { location: {
            href: 'http://board/stats.html?mode=tasks&uiVersion=older#heap',
            replace: url => navigations.push(url)
        } }
    });
    vm.runInContext(source, context);
    return { navigations, poll: data => context.reloadForFirmwareUpdate(data) };
}

const current = page('build-a');
assert.equal(current.poll({ firmwareVersion: 'build-a' }), false);
assert.equal(current.poll({}), false); // Older firmware does not cause reload loops.
assert.equal(current.navigations.length, 0);

// Detect an update even if it happened before the first successful status poll.
assert.equal(current.poll({ firmwareVersion: 'build-b' }), true);
assert.equal(current.poll({ firmwareVersion: 'build-b' }), true);
assert.equal(current.navigations.length, 1); // Overlapping polls navigate only once.
const target = new URL(current.navigations[0]);
assert.equal(target.pathname, '/stats.html');
assert.equal(target.searchParams.get('mode'), 'tasks');
assert.equal(target.searchParams.get('uiVersion'), 'build-b');
assert.equal(target.searchParams.getAll('uiVersion').length, 1);
assert.equal(target.hash, '#heap');

const refreshed = page('build-b');
assert.equal(refreshed.poll({ firmwareVersion: 'build-b' }), false);
assert.equal(refreshed.navigations.length, 0);
assert.equal(page(null).poll({ firmwareVersion: 'build-b' }), false);
console.log('Firmware reload tests passed');
