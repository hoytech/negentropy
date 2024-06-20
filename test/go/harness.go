package main

import (
	"strings"
	"bufio"
	"fmt"
	"os"
	"strconv"
	"encoding/hex"
	negentropy "github.com/illuzen/go-negentropy"
)

func split(s string, delim rune) []string {
	return strings.FieldsFunc(s, func(r rune) bool {
		return r == delim
	})
}

func main() {
	frameSizeLimit := uint64(0)
	if env, exists := os.LookupEnv("FRAMESIZELIMIT"); exists {
		var err error
		frameSizeLimit, err = strconv.ParseUint(env, 10, 64)
		if err != nil {
			panic(fmt.Errorf("invalid FRAMESIZELIMIT: %w", err))
		}
	}

	storage := negentropy.NewVector()
	var ne *negentropy.Negentropy

	scanner := bufio.NewScanner(os.Stdin)
	const maxCapacity = 1024 * 1024 // 1MB
	buf := make([]byte, maxCapacity)
	scanner.Buffer(buf, maxCapacity)

	for scanner.Scan() {
		line := scanner.Text()
		if len(line) == 0 {
			continue
		}

		items := split(line, ',')

		switch items[0] {
		case "item":
			if len(items) != 3 {
				panic("wrong num of fields")
			}
			created, err := strconv.ParseUint(items[1], 10, 64)
			if err != nil {
				panic(err)
			}
			id, err := hex.DecodeString(items[2]) // Assume fromHex translates hex string to []byte
			if err != nil {
				panic(err)
			}
			storage.Insert(created, id)

		case "seal":
			storage.Seal()
			neg, err := negentropy.NewNegentropy(storage, frameSizeLimit)
			if err != nil {
				panic(err)
			}
			ne = neg

		case "initiate":
			q, err := ne.Initiate()
			if err != nil {
				panic(err)
			}
			if frameSizeLimit != 0 && uint64(len(q)) > frameSizeLimit {
				panic("initiate frameSizeLimit exceeded")
			}
			fmt.Printf("msg,%s\n", hex.EncodeToString(q))

		case "msg":
			var q []byte
			if len(items) >= 2 {
				s, err := hex.DecodeString(items[1])
				if err != nil {
					panic(err)
				}
				q = s
			}

			if (*ne).IsInitiator {
				var have, need []string
				resp, err := ne.ReconcileWithIDs(q, &have, &need)
				if err != nil {
					panic(fmt.Sprintf("Reconciliation failed: %v", err))
				}

				for _, id := range have {
					fmt.Printf("have,%s\n", hex.EncodeToString([]byte(id)))
				}
				for _, id := range need {
					fmt.Printf("need,%s\n", hex.EncodeToString([]byte(id)))
				}

				if resp == nil {
					fmt.Println("done")
					continue
				}

				q = resp
			} else {
				s, err := ne.Reconcile(q)
				if err != nil {
					panic(fmt.Sprintf("Reconciliation failed: %v", err))
				}
				q = s
			}

			if frameSizeLimit > 0 && uint64(len(q)) > frameSizeLimit {
				panic("frameSizeLimit exceeded")
			}
			fmt.Printf("msg,%s\n", hex.EncodeToString(q))

		default:
			panic("unknown cmd: " + items[0])
		}
	}

	if err := scanner.Err(); err != nil {
		panic(err)
	}
}