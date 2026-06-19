// BrutePin v1.1 - Rust Wrapper (delegates to C engine)
use std::process::Command;

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let status = Command::new("./core/brutepin")
        .args(&args)
        .status()
        .expect("Failed to launch C engine");
    std::process::exit(status.code().unwrap_or(1));
}
