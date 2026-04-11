use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let patina_root = PathBuf::from(&manifest_dir).join("..").join("..");

    // Compile the C binding shim
    cc::Build::new()
        .cpp(true)
        .std("c++17")
        .file(patina_root.join("bindings/c/patina_c.cpp"))
        .include(&patina_root)
        .opt_level(2)
        .warnings(false)
        .compile("patina_c");

    // Link the C++ standard library
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    match target_os.as_str() {
        "macos" | "ios" => println!("cargo:rustc-link-lib=c++"),
        "linux" | "android" => println!("cargo:rustc-link-lib=stdc++"),
        "windows" => {} // MSVC links automatically
        _ => println!("cargo:rustc-link-lib=stdc++"),
    }

    // If a pre-built static lib exists, link it too
    let build_dir = patina_root.join("build");
    if build_dir.exists() {
        println!("cargo:rustc-link-search=native={}", build_dir.display());
        println!("cargo:rustc-link-lib=static=Patina");
    }

    // Rerun if the C binding source changes
    println!("cargo:rerun-if-changed=../../bindings/c/patina_c.h");
    println!("cargo:rerun-if-changed=../../bindings/c/patina_c.cpp");
}
