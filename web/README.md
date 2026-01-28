# ContestLogX Website

This is the ContestLogX website built with [Astro](https://astro.build) and [Tailwind CSS](https://tailwindcss.com).

## Development

```bash
# Install dependencies (first time only)
npm install

# Start development server
npm run dev

# The site will be available at http://localhost:4321
```

## Building for Production

```bash
# Build the static site
npm run build

# The output will be in the dist/ directory
```

## Deploying to Cloudflare Pages

### Option 1: Deploy from Command Line (Recommended)

```bash
# First time setup: Get your Cloudflare API token
# 1. Go to https://dash.cloudflare.com/profile/api-tokens
# 2. Create a token with "Cloudflare Pages - Edit" permissions
# 3. Set the environment variable:
export CLOUDFLARE_API_TOKEN="your-api-token-here"

# Or copy .env.example to .env and add your token there
cp .env.example .env
# Then edit .env and add your token

# Deploy to PRODUCTION (your main URL)
npm run deploy

# Or deploy to PREVIEW (temporary preview URL for testing)
npm run deploy:preview
```

**What's the difference?**
- `npm run deploy` → Deploys to **production** (your main contestlogx-web.pages.dev URL)
- `npm run deploy:preview` → Deploys to a **preview URL** (for testing before going live)

The deploy commands will:
1. Build the site (`npm run build`)
2. Deploy the `dist/` directory to your Cloudflare Pages project `contestlogx-web`
3. Show you the deployment URL in the terminal

**Note:** Add `CLOUDFLARE_API_TOKEN` to your shell profile (`.bashrc`, `.zshrc`) to persist it:
```bash
echo 'export CLOUDFLARE_API_TOKEN="your-token"' >> ~/.bashrc
```

### Option 2: Deploy via GitHub (Automatic)

1. Push your code to GitHub
2. Go to [Cloudflare Pages](https://pages.cloudflare.com)
3. Connect your GitHub repository
4. Configure build settings:
   - **Build command:** `npm run build`
   - **Build output directory:** `dist`
   - **Node version:** 18 or later

Cloudflare Pages will automatically build and deploy your site on every push.

## Project Structure

```
web/
├── src/
│   ├── layouts/
│   │   └── Layout.astro        # Main layout with navigation and footer
│   ├── pages/
│   │   ├── index.astro         # Home page
│   │   ├── screenshots.astro   # Screenshots gallery
│   │   ├── contests.astro      # Supported contests
│   │   ├── download.astro      # Download page
│   │   └── docs.astro          # Documentation
│   └── styles/
│       └── global.css          # Tailwind + custom styles
├── public/                     # Static assets (images, etc.)
├── dist/                       # Build output (git-ignored)
└── package.json
```

## Adding New Pages

1. Create a new `.astro` file in `src/pages/`
2. Import the Layout component
3. The page will automatically be available at `/<filename>`

Example:
```astro
---
import Layout from '../layouts/Layout.astro';
---

<Layout title="My New Page">
  <div class="container mx-auto px-4 py-10">
    <h1>My New Page</h1>
  </div>
</Layout>
```

## Customizing Styles

- Custom colors are defined in `src/styles/global.css`
- Use Tailwind utility classes for styling
- The site uses a dark GitHub-style theme

## Backup

The original HTML site is backed up in `../web-backup/`
