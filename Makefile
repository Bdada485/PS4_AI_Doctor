name: Build PS4 AI-Doctor

on: [push, workflow_dispatch]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Code
        uses: actions/checkout@v4

      - name: Build with OpenOrbis Toolchain
        uses: OpenOrbis/toolchain-action@main
        with:
          version: latest
          command: make

      - name: Upload Artifact
        uses: actions/upload-artifact@v4
        with:
          name: eboot
          path: eboot.bin
