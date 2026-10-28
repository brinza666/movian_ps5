//go:build ps5

// SPDX-License-Identifier: GPL-3.0-or-later

package main

import (
	"fmt"
	"log"
	"runtime"
	"time"

	"github.com/czz/movian-go/internal/arch"
	"github.com/czz/movian-go/internal/arch/ps5"
)

// renderLoop — PS5 payload main thread.
// SDL's video driver is main-thread affine, so this thread is locked
// and never calls into the GLW skin. Pad presses go through the same
// action names the GLW frontend injects (event.DispatchAction).
func renderLoop(ctx *appContext) {
	runtime.LockOSThread()

	arch.UILoopStart(ctx.propManager, ctx.eventManager)
	arch.SubscribeGlobalEventsink()
	stop := make(chan struct{})
	arch.ShutdownHookAdd(func(any, int) {
		select {
		case <-stop:
		default:
			close(stop)
		}
		arch.UILoopStopReq()
	}, nil, 1)

	go pumpNav(ctx, stop)

	if err := ps5.Open(1920, 1080); err != nil {
		log.Printf("[ps5] display: %v", err)
		log.Printf("[ps5] continuing without a picture; core is still up")
		<-stop
		return
	}
	defer ps5.Close()

	var last string
	tick := time.NewTicker(250 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case <-stop:
			return
		default:
		}
		for {
			code, ok := ps5.Poll()
			if !ok {
				break
			}
			if name, mapped := ps5.Action(code); mapped && ctx.eventManager != nil {
				log.Printf("[ps5] pad %s", name)
				ctx.eventManager.DispatchAction(name)
				if name == "Quit" {
					return
				}
			}
		}
		select {
		case <-stop:
			return
		case <-tick.C:
			page := statusPage(ctx)
			if page != last {
				ps5.SetStatus(page)
				last = page
			}
			ps5.Present()
		default:
			time.Sleep(8 * time.Millisecond)
		}
	}
}

func pumpNav(ctx *appContext, stop <-chan struct{}) {
	for {
		select {
		case <-stop:
			return
		case <-ctx.stopChan:
			return
		default:
		}
		if ctx.navSystem == nil {
			return
		}
		c := ctx.navSystem.NavCourier()
		if c == nil {
			return
		}
		c.WaitTimed(500)
	}
}

func statusPage(ctx *appContext) string {
	url := ""
	if ctx != nil && ctx.gconf != nil && ctx.gconf.InitialURL != "" {
		url = ctx.gconf.InitialURL
	}
	return fmt.Sprintf("MOVIAN GO\nPS5 payload  home /data/movian\nGL skin not started\nSDL framebuffer + DualSense\nCross activate   Circle back\nOptions menu     Triangle home\nurl %s\n", url)
}
