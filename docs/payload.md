# Loader ELF (port 9021)

The shipping product is the **home-screen title**, not a payload.

Sources for the old splicer payload live in
[`archive/splicer/`](../archive/splicer/). Binaries were removed from
git. Pre-alpha GitHub releases remain for archaeology only.

If you are debugging decode in isolation, send an ELF with Payload
Manager (`http://<ps5>:8084`) or elfldr **9021**. Do not send
`eboot.bin` there.

Titles hide the splash and run until the user closes them. Payloads that
`return` from `main` can SIGSEGV in the SDK CRT (`call __crt_start`).
That is unrelated to the Movian UI.
