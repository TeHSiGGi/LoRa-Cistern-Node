# Function to display the help message
help() {
    echo "Usage: setup.sh [command]"
    echo "Commands:"
    echo "  help: Display this help message"
    echo "  start: Start the phone interface"
    echo "  install: Install the python dependencies using the requirements.txt"
    echo "  systemd: Install the systemd service"
}

# Function to install the python dependencies
install() {
    echo "Installing python dependencies..."
    # Check if .venv directory does not exist
    if [ ! -d .venv ]; then
        echo "Creating virtual environment..."
        python3 -m venv .venv
    else
        echo "Virtual environment already exists"
    fi
    . .venv/bin/activate
    pip3 install -r requirements.txt
    echo "Python dependencies installed successfully"
}

start() {
    echo "Starting the phone interface..."
    export PYTHONPATH="$PYTHONPATH:$PWD"
    . .venv/bin/activate
    python3 -u lora_receiver.py
}

systemd() {
    echo "Setting up systemd service..."
    sudo cp systemd/* /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable --now lora-receiver.service
}

# Check if the user has provided a command
if [ $# -eq 0 ]; then
    echo "Error: No command provided"
    help
    exit 1
fi

# Check the command provided by the user
case $1 in
    help)
        help
        ;;
    install)
        install
        ;;
    start)
        start
        ;;
    systemd)
        systemd
        ;;
    *)
        echo "Error: Invalid command"
        help
        exit 1
        ;;
esac

exit 0