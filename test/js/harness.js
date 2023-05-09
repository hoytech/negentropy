const readline = require('readline');
const Negentropy = require('../../js/Negentropy.js');

const idSize = 16;

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
});

let n = 0;
let x1 = new Negentropy(idSize);
let x2 = new Negentropy(idSize);

rl.on('line', (line) => {
    let items = line.split(',');
    if (items.length !== 3) throw Error("too few items");

    let mode = parseInt(items[0]);
    let created = parseInt(items[1]);
    let id = Buffer.from(items[2].trim(), 'hex');
    if (id.byteLength !== idSize) throw Error(`id should be ${idSize} bytes`);

    if (mode === 1) {
        x1.addItem(created, id);
    } else if (mode === 2) {
        x2.addItem(created, id);
    } else if (mode === 3) {
        x1.addItem(created, id);
        x2.addItem(created, id);
    } else {
        throw Error("unexpected mode");
    }

    n++;
});

rl.once('close', () => {
    x1.seal();
    x2.seal();

    let q;
    let round = 0;

    while (true) {
        if (round === 0) {
            q = x1.initiate();
        } else {
            let [newQ, haveIds, needIds] = x1.reconcile(q);
            q = newQ;

            for (let id of haveIds) console.log(`xor,HAVE,${Buffer.from(id).toString('hex')}`);
            for (let id of needIds) console.log(`xor,NEED,${Buffer.from(id).toString('hex')}`);
        }

        if (q.length === 0) break;

        console.error(`[${round}] CLIENT -> SERVER: ${q.byteLength} bytes`);

        let [newQ, haveIds, needIds] = x2.reconcile(q);
        q = newQ;

        console.error(`[${round}] SERVER -> CLIENT: ${q.byteLength} bytes`);

        round++;
    }
});
