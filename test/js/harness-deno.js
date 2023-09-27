import { readline } from "https://deno.land/x/readline@v1.1.0/mod.ts";
import Negentropy from "../../js/Negentropy.mjs";

const idSize = 16;

let frameSizeLimit = 0;
if (Deno.env.get("FRAMESIZELIMIT")) frameSizeLimit = parseInt(Deno.env.get("FRAMESIZELIMIT"));

let ne = new Negentropy(idSize, frameSizeLimit);

for await (let line of readline(Deno.stdin)) {
    line = new TextDecoder().decode(line);
    let items = line.split(',');

    if (items[0] == "item") {
        if (items.length !== 3) throw Error("too few items");
        let created = parseInt(items[1]);
        let id = items[2].trim();
        ne.addItem(created, id);
    } else if (items[0] == "seal") {
        ne.seal();
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
}
