fn main() {
    println!("cargo::rustc-check-cfg=cfg(esp32s2)");
    println!(
        "cargo::rustc-env=LOG_LEVEL={}",
        if std::env::var("PROFILE").as_deref() == Ok("release") {
            "info"
        } else {
            "debug"
        }
    );
}
