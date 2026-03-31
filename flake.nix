{
  description = "Loom development environment";

  inputs.nixpkgs.url = "github:/NixOS/nixpkgs/nixos-25.11";

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
    llvm = pkgs.llvmPackages;
    clangStdenv = pkgs.overrideCC pkgs.stdenv llvm.libcxxClang;
    fmtLibcxx = pkgs.fmt.override { stdenv = clangStdenv; };
    doxygenClang = pkgs.doxygen.overrideAttrs (old: {
      buildInputs = old.buildInputs ++ [
        llvm.libllvm
        pkgs.libclang
      ];
      cmakeFlags = old.cmakeFlags ++ ["-Duse_libclang=ON"];
    });
  in {
    devShells.${system}.default = pkgs.mkShell {
      packages = with pkgs; [
        llvm.libcxxClang
        cmake
        fmtLibcxx
        glfw
        fast-float
        range-v3
        hwloc
        pkgconf
        # docs generation
        graphviz
        doxygenClang
        texliveBasic
      ];

      shellHook = ''
      export CMAKE_EXPORT_COMPILE_COMMANDS=1
      export NIX_ENFORCE_NO_NATIVE=0
      '';
    };
  };
}
