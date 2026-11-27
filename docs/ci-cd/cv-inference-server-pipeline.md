# cv-inference-server — Documentation publishing pipeline

How the Doxygen HTML documentation is built and published to **both** GitHub
Pages and GitLab Pages. The project is CPU-only and free-tier safe: the entire
docs toolchain is plain apt packages, and no source / CMake / Doxygen config is
changed to publish.

> Scope: this document covers only the *docs publishing* jobs. The build-test,
> sanitizer, and release pipelines are documented alongside their own files
> (`.github/workflows/ci.yml`, `.github/workflows/release.yml`, `.gitlab-ci.yml`).

## Docs toolchain

| Package    | Why                                                      |
|------------|----------------------------------------------------------|
| `doxygen`  | The documentation generator.                             |
| `graphviz` | Provides `dot` for Doxygen call / collaboration graphs.  |
| `plantuml` | Renders the C4 component diagram (pulls a JRE).          |

PlantUML's bundled C4 standard library resolves **offline**, so the diagram
needs no network access.

## Build commands (identical everywhere — CI and local)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVIS_BUILD_DOCS=ON
cmake --build build --target docs
```

Output HTML lands in `build/docs/html/` (entry point `index.html`); the C4
component diagram is `build/docs/html/cvis_c4_component.svg`.

### Reproduce locally

```sh
sudo apt-get install -y doxygen graphviz plantuml
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCVIS_BUILD_DOCS=ON
cmake --build build --target docs
xdg-open build/docs/html/index.html
```

## GitHub Pages — `.github/workflows/docs.yml`

Dedicated `Docs` workflow (kept out of `ci.yml`), triggered on push to `main`
(path-filtered to docs/headers/CMake changes) and `workflow_dispatch`.

- **`build` job** — installs the docs toolchain, runs the two cmake commands,
  then uploads `build/docs/html` via `actions/upload-pages-artifact`.
- **`deploy` job** — publishes the artifact with `actions/deploy-pages` into the
  `github-pages` environment.
- Top-level `permissions` grant `pages: write`, `id-token: write`,
  `contents: read`; a `concurrency` group `pages` (with
  `cancel-in-progress: false`) serialises deployments per GitHub's documented
  pattern.

Enable once in the repo: **Settings → Pages → Build and deployment →
Source: GitHub Actions**.

## GitLab Pages — `pages` job in `.gitlab-ci.yml`

GitLab's Pages contract requires a job literally named **`pages`** that leaves
the published site in a **`public/`** artifact directory.

- Runs in a new `pages` stage, restricted to the default branch via
  `rules: - if: $CI_COMMIT_BRANCH == $CI_DEFAULT_BRANCH`.
- Its `before_script` extends the global one (`!reference [default,
  before_script]`, which installs cmake/g++) with the docs toolchain — it does
  not override it, so the existing build-test/sanitizer jobs are unaffected.
- Builds the docs, moves `build/docs/html` → `public/`, and declares
  `artifacts: paths: [public]`.
- No `.deps_cache`: the docs build has no FetchContent, so caching would add
  overhead for no benefit.

GitLab serves the site automatically once the `pages` job succeeds on the
default branch (see **Deploy → Pages** in the GitLab project).
