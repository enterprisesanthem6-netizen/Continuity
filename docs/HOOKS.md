# Hooks yang harus dicari di libminecraftpe.so

Mod ini sudah lengkap di sisi logika (aturan CTM, 47 tile, konversi pack, tes).
Yang belum ada hanya **pengikatan ke game**: empat alamat di `src/Signatures.hpp`.
Selama kosong, mod tidak melakukan apa pun dan tidak menyentuh game.

| Pattern | Apa itu | Cara mengenali |
|---|---|---|
| `kTessellateBlockPattern` | Fungsi yang dipanggil per blok saat mesh chunk dibangun | Dipanggil ribuan kali per chunk; menerima BlockSource, BlockPos, Block |
| `kFaceUvPattern` | Pengambil `TextureUVCoordinateSet` untuk satu sisi blok | Menerima Block + indeks sisi (0-5), mengembalikan pointer UV |
| `kBlockIdAtPattern` | Baca blok pada posisi (BlockSource::getBlock) + nama blok | Dipanggil dari tessellator dengan BlockPos tetangga |
| `kAtlasLookupPattern` | Kunci `terrain_texture.json` -> UV | Mengacu string seperti "glass" / nama tile terrain |

## Cara mencari (di Termux, memakai tools repo)

1. Ambil `libminecraftpe.so` versi yang dipakai.
2. Cari string tile (misal "terrain_texture", "glass") dengan `strings`, lalu
   cari xref ke fungsinya lewat `python3 tools/disasm.py`.
3. Turunkan pattern: `python3 tools/derive_pattern.py --lib OLD.so --addr 0xADDR --find-in NEW.so`
4. Pastikan unik di semua versi: `python3 tools/verify_signatures.py libminecraftpe.so`
5. Isi `Signatures.hpp`, lalu tulis pengikatan di `bindGame()` (`src/GameBridge.cpp`,
   langkah 1-4 ada di komentarnya).
