/*
 * ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 *
 * kdos-theme-helper — build and apply a full COSMIC theme from a small
 * palette, using cosmic-theme's own derivation logic so every component
 * colour (hover, pressed, dividers, …) comes out exactly as the Settings UI
 * would compute it.
 *
 *   kdos-theme-helper <accent> <bg> <container> <text> <secondary> <urgent>
 *
 * All colours are RRGGBB hex. Writes the applied Dark theme AND the theme
 * builder (so cosmic-settings' appearance page starts from the same values),
 * into $XDG_CONFIG_HOME/cosmic/ — point XDG_CONFIG_HOME at /etc/skel/.config
 * to seed the image at build time.
 */

use cosmic_config::CosmicConfigEntry;
use cosmic_theme::palette::{Srgb, Srgba};
use cosmic_theme::{Theme, ThemeBuilder, ThemeMode};

fn hex(s: &str) -> (f32, f32, f32) {
    let v = u32::from_str_radix(s.trim_start_matches('#'), 16).expect("bad hex colour");
    (
        ((v >> 16) & 0xff) as f32 / 255.0,
        ((v >> 8) & 0xff) as f32 / 255.0,
        (v & 0xff) as f32 / 255.0,
    )
}

fn srgb(s: &str) -> Srgb {
    let (r, g, b) = hex(s);
    Srgb::new(r, g, b)
}

fn srgba(s: &str) -> Srgba {
    let (r, g, b) = hex(s);
    Srgba::new(r, g, b, 1.0)
}

fn main() {
    let a: Vec<String> = std::env::args().collect();
    if a.len() != 7 {
        eprintln!("usage: kdos-theme-helper <accent> <bg> <container> <text> <secondary> <urgent>");
        std::process::exit(1);
    }

    let builder = ThemeBuilder::dark()
        .accent(srgb(&a[1]))
        .bg_color(srgba(&a[2]))
        .primary_container_bg(srgba(&a[3]))
        .text_tint(srgb(&a[4]))
        .success(srgb(&a[1]))
        .warning(srgb(&a[5]))
        .destructive(srgb(&a[6]));

    builder
        .clone()
        .write_entry(&ThemeBuilder::dark_config().expect("builder config"))
        .expect("write builder");

    let theme = builder.build();
    theme
        .write_entry(&Theme::dark_config().expect("theme config"))
        .expect("write theme");

    // Dark mode on, so the theme above is the one every component reads.
    let mode_cfg = ThemeMode::config().expect("mode config");
    let mut mode = ThemeMode::get_entry(&mode_cfg).unwrap_or_default();
    mode.is_dark = true;
    mode.write_entry(&mode_cfg).expect("write mode");

    println!("applied");
}
