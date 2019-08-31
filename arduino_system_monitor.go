package main

import (
  "fmt"
  "github.com/tarm/serial"
  "github.com/shirou/gopsutil/cpu"
  "github.com/shirou/gopsutil/mem"
  "time"
  "os/exec"
  "io/ioutil"
  "runtime"
  "strings"
  "github.com/getlantern/systray"
  "syscall"
)

func sendTime(messages chan string) {
  for true {
    t := time.Now()
    _, offset := t.Zone()
    unixtime := t.Unix()
    unixtime_shifted := unixtime + int64(offset);
    str := fmt.Sprintf("T%d\n", unixtime_shifted);
    messages <- str;
    time.Sleep(1 * time.Second);
  }
}

func sendSystemStats(messages chan string) {
  for true {
    v, _ := mem.VirtualMemory()
    memPercent := v.UsedPercent
    c, _ := cpu.Percent(0, false)
    cpuPercent := c[0]
    cpuString := fmt.Sprintf("C%d\n", int(cpuPercent))
    messages <- cpuString
    memString := fmt.Sprintf("M%d\n", int(memPercent))
    messages <- memString
    time.Sleep(1 * time.Second)
  }
}

func getSpotifyInfo() (string, string) {
  if runtime.GOOS == "windows" {
    tasklist := exec.Command("tasklist", "/V", "/FI", "IMAGENAME eq spotify.exe", "/FO", "CSV")
    tasklist.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
    stdout, err := tasklist.StdoutPipe()
    if err != nil {
      return "", ""
    }
    if err := tasklist.Start(); err != nil {
      return "", ""
    }
    buf, err := ioutil.ReadAll(stdout)
    if err != nil {
      return "", ""
    }
    if err := tasklist.Wait(); err != nil {
      return "", ""
    }
    s := string(buf)
    lines := strings.Split(s, "\r\n")
    if len(lines) <= 2 {
      return "", ""
    }
    for _, line := range lines[1:] {
      if line == "" {
        continue
      }
      parts := strings.Split(line, ",")
      title := parts[len(parts) - 1]
      if len(title) < 2 {
        continue
      }
      title = title[1:len(title)-1]
      if title == "N/A" {
        continue
      } else if title == "AngleHiddenWindow" {
        continue
      }
      titleParts := strings.Split(title, " - ")
      if len(titleParts) != 2 {
        return title, ""
      } else {
        return titleParts[1], titleParts[0]
      }
    }
  }
  return "", ""
}

func sendMusicTrackName(messages chan string) {
  for true {
    spotifyTrack, spotifyArtist := getSpotifyInfo()
    if spotifyTrack != "" {
      messages <- fmt.Sprintf("N%s\n", spotifyTrack)
      messages <- fmt.Sprintf("A%s\n", spotifyArtist)
      messages <- fmt.Sprintf("PSpotify\n")
    } else {
      messages <- fmt.Sprintf("N\n");
      messages <- fmt.Sprintf("A\n");
      messages <- fmt.Sprintf("P\n");
    }
    time.Sleep(1 * time.Second)
  }
}

func waitForQuit(quitMenuItem *systray.MenuItem) {
  <-quitMenuItem.ClickedCh
  systray.Quit()
}

func onReady() {
  systray.SetTitle("Monitor")
  systray.SetTooltip("Arduino System Monitor")
  quitMenuItem := systray.AddMenuItem("Quit", "Quit the monitor.")
  go waitForQuit(quitMenuItem)

  messages := make(chan string)
  go sendTime(messages)
  go sendSystemStats(messages)
  go sendMusicTrackName(messages)

  for true {
    serialConfig := &serial.Config{Name: "COM4", Baud: 9600}
    port, err := serial.OpenPort(serialConfig)
    if err != nil {
      time.Sleep(1 * time.Second);
      continue;
    }

    for true {
      msg := <-messages
      bytes := []byte(msg)
      _, err := port.Write(bytes)
      if err != nil {
        break;
      }
    }
  }
}

func onExit() {
}

func main() {
  systray.Run(onReady, onExit);
}
