package main

import (
	"bufio"
	"fmt"
	"math"
	"os"
	"strconv"
	"strings"

	"github.com/nbd-wtf/go-nostr"
	"github.com/nbd-wtf/go-nostr/nip77/negentropy"
)

func main() {
	frameSizeLimit, _ := strconv.Atoi(os.Getenv("FRAMESIZELIMIT"))
	if frameSizeLimit == 0 {
		frameSizeLimit = math.MaxInt
	}

	neg := negentropy.NewNegentropy(negentropy.NewVector(), frameSizeLimit)

	have := make([]string, 0, 500)
	need := make([]string, 0, 500)

	go func() {
		for item := range neg.Haves {
			have = append(have, item)
		}
	}()
	go func() {
		for item := range neg.HaveNots {
			need = append(need, item)
		}
	}()

	scanner := bufio.NewScanner(os.Stdin)
	const maxCapacity = 1024 * 1024 // 1MB
	buf := make([]byte, maxCapacity)
	scanner.Buffer(buf, maxCapacity)

	for scanner.Scan() {
		line := scanner.Text()
		if len(line) == 0 {
			continue
		}

		items := strings.Split(line, ",")

		switch items[0] {
		case "item":
			if len(items) != 3 {
				panic("wrong num of fields")
			}
			created, err := strconv.ParseUint(items[1], 10, 64)
			if err != nil {
				panic(err)
			}
			neg.Insert(&nostr.Event{CreatedAt: nostr.Timestamp(created), ID: items[2]})

		case "seal":
			// do nothing

		case "initiate":
			q := neg.Initiate()
			if frameSizeLimit != 0 && len(q)/2 > frameSizeLimit {
				panic("initiate frameSizeLimit exceeded")
			}
			fmt.Printf("msg,%s\n", q)

		case "msg":
			q, err := neg.Reconcile(items[1])
			if err != nil {
				panic(fmt.Sprintf("Reconciliation failed: %v", err))
			}
			if q == "" {
				fmt.Println("done")

				for _, id := range have {
					fmt.Printf("have,%s\n", id)
				}
				for _, id := range need {
					fmt.Printf("need,%s\n", id)
				}

				continue
			}

			if frameSizeLimit > 0 && len(q)/2 > frameSizeLimit {
				panic("frameSizeLimit exceeded")
			}

			fmt.Printf("msg,%s\n", q)

		default:
			panic("unknown cmd: " + items[0])
		}
	}

	if err := scanner.Err(); err != nil {
		panic(err)
	}
}
