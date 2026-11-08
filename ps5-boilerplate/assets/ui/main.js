# Movian PS5 UI - Main JavaScript Entry Point
# This file would be placed at app0:/assets/ui/main.js
# It uses Duktape JS engine with PS5 native bindings

print("Movian PS5 UI loading...");

// PS5 API bindings (provided by native code)
var ps5 = {
    video: {
        framebuffer: function() { return ps5.video.framebuffer(); },
        flip: function() { ps5.video.flip(); }
    },
    audio: {
        write: function(pcm) { ps5.audio.write(pcm); }
    },
    input: {
        poll: function() { return ps5.input.poll(); }
    },
    fs: {
        read: function(path) { return ps5.fs.read(path); },
        write: function(path, data) { ps5.fs.write(path, data); }
    },
    net: {
        smb2_connect: function(host, share) { return ps5.net.smb2_connect(host, share); }
    },
    log: function(msg) { ps5.log(msg); }
};

// Movian application state
var movian = {
    version: "0.1.0",
    running: true,
    width: 1920,
    height: 1080,
    framebuffer: null,
    
    init: function() {
        ps5.log("Movian PS5 UI initializing...");
        
        // Get video framebuffer
        this.framebuffer = ps5.video.framebuffer();
        ps5.log("Framebuffer: " + this.framebuffer);
        
        // Initialize UI
        this.renderBackground();
        
        ps5.log("Movian PS5 UI ready");
    },
    
    renderBackground: function() {
        // Clear screen to dark blue (Movian branding)
        var fb = this.framebuffer;
        if (!fb) return;
        
        // Fill with Movian dark blue #1a1a2e
        for (var y = 0; y < this.height; y++) {
            for (var x = 0; x < this.width; x++) {
                var offset = (y * this.width + x) * 4;
                fb[offset] = 0x1a;     // R
                fb[offset + 1] = 0x1a; // G
                fb[offset + 2] = 0x2e; // B
                fb[offset + 3] = 0xff; // A
            }
        }
    },
    
    onRender: function() {
        // Called each frame by native loop
        ps5.video.flip();
    },
    
    onInput: function(input) {
        // Handle controller input
        if (input.cross) {
            ps5.log("Cross button pressed");
        }
        if (input.circle) {
            ps5.log("Circle button pressed");
        }
        if (input.square) {
            ps5.log("Square button pressed");
        }
        if (input.triangle) {
            ps5.log("Triangle button pressed");
        }
    }
};

// Initialize on load
movian.init();

ps5.log("Movian PS5 UI loaded successfully");
