package main

import (
	"fmt"
	"sync"
)

func producer(work chan int, coord chan int, lo, hi int) {
	for n := lo; n <= hi; n++ {
		work <- n
	}
	coord <- 1
}

func main() {
	work := make(chan int, 64)
	coord := make(chan int, 4)
	var wg sync.WaitGroup
	wg.Add(2)
	go func() { defer wg.Done(); producer(work, coord, 1, 30) }()
	go func() { defer wg.Done(); producer(work, coord, 31, 60) }()

	go func() {
		for i := 0; i < 2; i++ {
			<-coord
		}
		close(work)
	}()

	sum := 0
	for v := range work {
		sum += v
	}
	wg.Wait()
	fmt.Println(sum)
}
