# Client licensing

The client mod in this folder (`client/`) is licensed under the **GNU General Public License v3.0 or later**
(`LICENSE` here). It is a plugin loaded into RogueSquadron64Recomp's process and built against its GPL-3.0
`include/rs64/host_api.h`, so it follows the host's license.

Everything else in the RS64AP repository (the apworld, the shared data, the tools and the docs) is MIT-licensed
(`../LICENSE`).

Third-party code fetched at build time and linked into the client:

| Project | License |
| --- | --- |
| [apclientpp](https://github.com/black-sliver/apclientpp) | MIT |
| [wswrap](https://github.com/black-sliver/wswrap) | MIT |
| [websocketpp](https://github.com/zaphoyd/websocketpp) | BSD-3-Clause |
| [asio](https://github.com/chriskohlhoff/asio) | Boost Software License 1.0 |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT |
| [OpenSSL](https://www.openssl.org/) 3.x | Apache-2.0 |
| zlib (optional) | zlib |

All of these are compatible with GPL-3.0. Release zips should include their license texts.
