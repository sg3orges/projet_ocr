# OCR Project

An OCR (Optical Character Recognition) project for solving word search puzzles from images.

## Features

- **Image Processing**: Automatic rotation and cleaning of input images
- **Grid Detection**: Detects letter grids from images
- **Word Recognition**: Uses a neural network to recognize individual letters
- **Word Search Solver**: Automatically finds words in the detected grid

## Prerequisites

- GCC compiler
- GTK3 development libraries
- SDL2 and SDL2_image libraries
- Make

## Installation

### macOS (Homebrew)
```bash
brew install gtk+3 sdl2 sdl2_image
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install libgtk-3-dev libsdl2-dev libsdl2-image-dev
```

## Building the Project

To compile the project, simply run:

```bash
make
```

This will compile all source files and create the `ocr_project` executable.

To clean the build files:

```bash
make clean
```

## Usage

### Running the Application

1. **Launch the application:**
   ```bash
   ./ocr_project
   ```

2. **Select an image:**
   - When the interface opens, click on the **"Image"** button
   - A gallery of sample images will be displayed
   - Click **"Choose"** on the image you want to process

3. **Process the image:**
   - The rotation interface will open automatically
   - Use the tools to adjust the image if needed:
     - **Load Image**: Load a different image
     - **Auto Rotate**: Automatically rotate the image
     - **Clean**: Remove noise from the image
   - Click **"Next"** to proceed to detection

4. **Solve the puzzle:**
   - The detection interface will display the processed image
   - Click the **"Solve"** button to:
     - Detect the letter grid
     - Recognize letters using the neural network
     - Find all words in the grid
   - The solution will be displayed with colored boxes highlighting each found word

## Project Structure

```
projet_ocr/
├── main.c                  # Main entry point
├── interface/              # Main GUI interface
├── rotations/              # Image rotation and cleaning
├── detectionV2/            # Grid and letter detection
├── neuronne/               # Neural network for letter recognition
├── solver/                 # Word search solver
├── Exemples_dimages/       # Sample images
└── Makefile               # Build configuration
```

## Neural Network

The project includes a neural network trained to recognize letters A-Z. The network is automatically trained on first run if no saved model exists. The trained model is saved in `neuronne/brain.bin`.

## Sample Images

Sample images are provided in the `Exemples_dimages/` directory. These include various word search puzzles at different difficulty levels.

## Output

The program generates several output files during processing:
- `images/`: Processed images
- `cells/`: Individual letter cells extracted from the grid
- `letterInWord/`: Letters grouped by detected words
- `GRIDL`: Detected letter grid in text format
- `GRIDWO`: List of words to find
- `CELLPOS`: Cell position information

## Troubleshooting

- **Compilation errors**: Ensure all required libraries are installed
- **No images displayed**: Check that the `Exemples_dimages/` directory exists and contains images
- **Neural network training**: First run may take longer as the network trains on the dataset

## Authors

See the `AUTHORS` file for contributor information.

## License

This project is part of an academic assignment at EPITA.
