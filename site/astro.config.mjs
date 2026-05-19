import { defineConfig } from 'astro/config';

export default defineConfig({
  site: 'https://eliseucbrito.github.io',
  base: '/battle-cin',
  outDir: '../docs',
  build: {
    format: 'file',
  },
});
