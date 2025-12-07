{
  description = "Loom development environment";

  inputs.nixpkgs.url = "github:/NixOS/nixpkgs/nixos-25.11";

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
    clangStdenv = pkgs.overrideCC pkgs.stdenv pkgs.llvmPackages_20.libcxxClang;
    fmtLibcxx = pkgs.fmt.override { stdenv = clangStdenv; };
  in {
    devShells.${system}.default = pkgs.mkShell {
      packages = with pkgs; [
        llvmPackages_20.libcxxClang
        cmake
        fmtLibcxx
        glfw
        fast-float
        range-v3
        graphviz                # docs
        doxygen                 # docs
      ];

      shellHook = ''
      export CMAKE_EXPORT_COMPILE_COMMANDS=1
      export NIX_ENFORCE_NO_NATIVE=0
      '';
    };
  };
}
