const readline = require('readline');
const { Negentropy, NegentropyStorageVector } = require('../../js/Negentropy.js');

let frameSizeLimit = 0;
if (process.env.FRAMESIZELIMIT) frameSizeLimit = parseInt(process.env.FRAMESIZELIMIT);

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
});

let ne;
let storage = new NegentropyStorageVector();

rl.on('line', async (line) => {
    let items = line.split(',');

    if (items[0] == "item") {
        if (items.length !== 3) throw Error("too few items");
        let created = parseInt(items[1]);
        let id = items[2].trim();
        storage.addItem(created, id);
    } else if (items[0] == "seal") {
        storage.seal();
        ne = new Negentropy(storage, frameSizeLimit);
    } else if (items[0] == "initiate") {
        let q = await ne.initiate();
        if (frameSizeLimit && q.length/2 > frameSizeLimit) throw Error("frameSizeLimit exceeded");
        console.log(`msg,${q}`);
    } else if (items[0] == "msg") {
        let q = items[1];
        let [newQ, haveIds, needIds] = await ne.reconcile(q);
        q = newQ;

        for (let id of haveIds) console.log(`have,${id}`);
        for (let id of needIds) console.log(`need,${id}`);

        if (frameSizeLimit && q !== null && q.length/2 > frameSizeLimit) throw Error("frameSizeLimit exceeded");

        if (q === null) {
            console.log(`done`);
        } else {
            console.log(`msg,${q}`);
        }
    } else {
        throw Error(`unknown cmd: ${items[0]}`);
    }
});

rl.on('close', () => {
    process.exit(0);
});
