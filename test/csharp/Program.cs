
using Negentropy;
using System;
using System.Diagnostics;

var limit = Environment.GetEnvironmentVariable("FRAMESIZELIMIT");

var options = new NegentropyOptions
{
    FrameSizeLimit = string.IsNullOrEmpty(limit) ? 0 : uint.Parse(limit)
};

var builder = new NegentropyBuilder(options);
Negentropy.Negentropy? ne = null;

while (true)
{
    var line = Console.ReadLine();

    if (line == null)
    {
      continue;
    }

    var items = line.Split(",");

    switch (items[0])
    {
        case "item":
            if (items.Length != 3) throw new ArgumentException("Too few items for 'item'");
            
            var created = long.Parse(items[1]);
            var id = items[2].Trim();
            var item = new StorageItem(id, created);

            builder.Add(item);

            break;
        case "seal":
            ne = builder.Build();
            break;
        case "initiate":
            var q = ne?.Initiate() ?? "";
            if (options.FrameSizeLimit > 0 && q.Length / 2 > options.FrameSizeLimit) throw new InvalidOperationException("FrameSizeLimit exceeded");
            Console.WriteLine($"msg,{q}");
            break;
        case "msg":
            var newQ = items[1];

            if (ne == null) throw new InvalidOperationException("Negentropy not initialized");

            var result = ne.Reconcile(newQ);
            
            newQ = result.Query;

            foreach (var x in result.HaveIds) Console.WriteLine($"have,{x}");
            foreach (var x in result.NeedIds) Console.WriteLine($"need,{x}");

            if (options.FrameSizeLimit > 0 && newQ.Length / 2 > options.FrameSizeLimit) throw new InvalidOperationException("FrameSizeLimit exceeded");

            if (newQ == string.Empty)
            {
                Console.WriteLine("done");
            }
            else
            {
                Console.WriteLine($"msg,{newQ}");
            }
            break;
        default:
            return;
    }
}

record StorageItem(string Id, long Timestamp) : INegentropyItem { }