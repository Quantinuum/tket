/**
 * Copy as Markdown functionality for TKET documentation pages.
 *
 * Fetches the clean Markdown source from Sphinx's _sources/ endpoint and
 * copies it to the user's clipboard, providing visual feedback.
 */

(function () {
  'use strict';

  function initCopyAsMarkdown() {
    const buttons = document.querySelectorAll('.copy-as-markdown-btn');
    if (!buttons.length) return;

    buttons.forEach((btn) => {
      // Prevent duplicate event listener attachment
      if (btn.dataset.initialized === 'true') return;
      btn.dataset.initialized = 'true';

      btn.addEventListener('click', async function (e) {
        e.preventDefault();
        const sourceUrl = btn.getAttribute('data-source-url');
        if (!sourceUrl) {
          console.warn('[Copy as Markdown] No data-source-url attribute found.');
          return;
        }

        const defaultLabel = btn.querySelector('.btn-text');
        const defaultTitle = btn.getAttribute('title') || 'Copy page as Markdown';

        try {
          // Fetch raw markdown source
          const response = await fetch(sourceUrl);
          if (!response.ok) {
            throw new Error(`Failed to load source: HTTP ${response.status}`);
          }
          const markdownText = await response.text();

          // Write to clipboard
          if (navigator.clipboard && navigator.clipboard.writeText) {
            await navigator.clipboard.writeText(markdownText);
          } else {
            // Fallback for older browsers or non-secure contexts
            const textarea = document.createElement('textarea');
            textarea.value = markdownText;
            textarea.style.position = 'fixed';
            textarea.style.top = '-9999px';
            textarea.style.left = '-9999px';
            document.body.appendChild(textarea);
            textarea.focus();
            textarea.select();
            const successful = document.execCommand('copy');
            document.body.removeChild(textarea);
            if (!successful) {
              throw new Error('Fallback execCommand failed');
            }
          }

          // Visual feedback on success
          btn.classList.add('copy-success');
          btn.setAttribute('title', 'Copied!');
          if (defaultLabel) defaultLabel.textContent = 'Copied!';

          setTimeout(() => {
            btn.classList.remove('copy-success');
            btn.setAttribute('title', defaultTitle);
            if (defaultLabel) defaultLabel.textContent = 'Copy Markdown';
          }, 2000);
        } catch (err) {
          console.error('[Copy as Markdown] Error:', err);
          btn.classList.add('copy-failure');
          btn.setAttribute('title', 'Failed to copy');
          if (defaultLabel) defaultLabel.textContent = 'Failed to copy';

          setTimeout(() => {
            btn.classList.remove('copy-failure');
            btn.setAttribute('title', defaultTitle);
            if (defaultLabel) defaultLabel.textContent = 'Copy Markdown';
          }, 2500);
        }
      });
    });
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initCopyAsMarkdown);
  } else {
    initCopyAsMarkdown();
  }
})();
