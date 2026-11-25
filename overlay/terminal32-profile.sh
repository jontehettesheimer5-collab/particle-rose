# Terminal32 additions by 32e4, GPL-3.0-or-later.
if [ -x /root/.venv/bin/python ]; then
    export PATH="/root/.venv/bin:$PATH"
fi
case "$-" in
    *i*)
        printf '\nTerminal32 · powered by iSH\n'
        if [ ! -f /root/.terminal32-ready ]; then
            printf 'Run setup-dev to install Python, pip, requests, Git, curl and nano.\n'
            printf 'The initial setup needs an internet connection.\n'
        fi
        ;;
esac
