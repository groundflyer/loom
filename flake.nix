{
  description = "Loom development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }: let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in {
    devShells.${system}.default = pkgs.mkShell {
      packages = with pkgs; [
        clang
        libcxx
        libcxxabi
        cmake
        fmt
        glfw
        fast-float
      ];

      shellHook = ''
      export CMAKE_EXPORT_COMPILE_COMMANDS=1
      '';
    };
  };
}
