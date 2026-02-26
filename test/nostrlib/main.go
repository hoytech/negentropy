package main

import (
	"bufio"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"

	"fiatjaf.com/nostr"
	"fiatjaf.com/nostr/nip77/negentropy"
	"fiatjaf.com/nostr/nip77/negentropy/storage/vector"
)

func main() {
	frameSizeLimit, _ := strconv.Atoi(os.Getenv("FRAMESIZELIMIT"))

	vec := vector.New()
	neg := negentropy.New(vec, frameSizeLimit, true, true)

	have := make([]nostr.ID, 0, 500)
	need := make([]nostr.ID, 0, 500)

	wg := sync.WaitGroup{}
	wg.Add(2)
	go func() {
		for item := range neg.Haves {
			have = append(have, item)
		}
		wg.Done()
	}()
	go func() {
		for item := range neg.HaveNots {
			need = append(need, item)
		}
		wg.Done()
	}()

	scanner := bufio.NewScanner(os.Stdin)
	const maxCapacity = 1024 * 1024 * 16 // 16MB
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
			created, err := strconv.ParseUint(items[1], 10, 64)
			if err != nil {
				panic(err)
			}
			vec.Insert(nostr.Timestamp(created), nostr.MustIDFromHex(items[2]))

		case "seal":
			vec.Seal()

		case "initiate":
			q := neg.Start()
			if frameSizeLimit != 0 && len(q)/2 > frameSizeLimit {
				panic("frameSizeLimit exceeded")
			}
			fmt.Printf("msg,%s\n", q)

		case "msg":
			q, err := neg.Reconcile(items[1])
			if err != nil {
				panic(fmt.Sprintf("reconciliation failed: %v", err))
			}
			if q == "" {
				wg.Wait()

				for _, id := range have {
					fmt.Printf("have,%s\n", id.Hex())
				}
				for _, id := range need {
					fmt.Printf("need,%s\n", id.Hex())
				}

				fmt.Println("done")

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
