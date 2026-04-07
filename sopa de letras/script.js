// Constants
const LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const DIRECTIONS = [
    { x: 1, y: 0, name: 'horizontal' },        // Right
    { x: 0, y: 1, name: 'vertical' },          // Down
    { x: 1, y: 1, name: 'diagonal-down' },     // Down-Right
    { x: -1, y: 1, name: 'diagonal-up' },      // Down-Left
    { x: -1, y: 0, name: 'horizontal-rev' },   // Left (reverse)
    { x: 0, y: -1, name: 'vertical-rev' },     // Up (reverse)
    { x: -1, y: -1, name: 'diag-down-rev' },   // Up-Left (reverse)
    { x: 1, y: -1, name: 'diag-up-rev' }       // Up-Right (reverse)
];

// State
let currentGrid = [];
let gridSize = 15;
let placedWords = []; // Array of { word, startX, startY, dirX, dirY, path: [{x,y}] }
let isSolving = false;

// DOM Elements
const wordsInput = document.getElementById('words-input');
const wordCountDisplay = document.getElementById('word-count-display');
const difficultySelect = document.getElementById('difficulty');
const autoSizeCheckbox = document.getElementById('grid-size-auto');
const manualSizeContainer = document.getElementById('manual-size-container');
const gridSizeInput = document.getElementById('grid-size');
const gridSizeDisplay = document.getElementById('grid-size-display');
const generateBtn = document.getElementById('generate-btn');
const outputArea = document.getElementById('output-area');
const wordGridElement = document.getElementById('word-grid');
const wordListElement = document.getElementById('word-list');
const printBtn = document.getElementById('print-btn');
const pdfBtn = document.getElementById('pdf-btn');
const solveBtn = document.getElementById('solve-btn');
const errorToast = document.getElementById('error-message');
const randomWordsBtn = document.getElementById('random-words-btn');

let wordBank = [];

// Event Listeners
document.addEventListener('DOMContentLoaded', () => {
    wordsInput.addEventListener('input', updateWordCount);
    
    autoSizeCheckbox.addEventListener('change', (e) => {
        manualSizeContainer.style.display = e.target.checked ? 'none' : 'flex';
    });

    gridSizeInput.addEventListener('input', (e) => {
        gridSizeDisplay.textContent = `${e.target.value}x${e.target.value}`;
    });

    generateBtn.addEventListener('click', generatePuzzle);
    printBtn.addEventListener('click', () => window.print());
    pdfBtn.addEventListener('click', generatePDF);
    solveBtn.addEventListener('click', toggleSolution);
    randomWordsBtn.addEventListener('click', fillRandomWords);

    initWordBank();

    // Initial words for demonstration
    wordsInput.value = "ROBOTICA\nPROGRAMACION\nSENSORES\nMOTORES\nARDUINO\nINTELIGENCIA\nALGORITMO";
    updateWordCount();
});

function updateWordCount() {
    const count = getCleanWords().length;
    wordCountDisplay.textContent = count;
    if (count > 30) {
        wordCountDisplay.classList.add('limit-reached');
    } else {
        wordCountDisplay.classList.remove('limit-reached');
    }
}

function getCleanWords() {
    const raw = wordsInput.value;
    // Split by comma or newline, trim spaces, convert to uppercase, remove accents, remove empty
    return raw.split(/[\n,]+/)
        .map(w => w.trim().toUpperCase()
            .normalize("NFD").replace(/[\u0300-\u036f]/g, "") // remove accents
            .replace(/[^A-Z]/g, '') // keep only A-Z
        )
        .filter(w => w.length >= 2);
}

function initWordBank() {
    try {
        const saved = localStorage.getItem('sopaDeLetrasWordBank');
        if (saved) {
            wordBank = JSON.parse(saved);
        } else {
            wordBank = typeof defaultWordBank !== 'undefined' ? [...defaultWordBank] : ["ROBOTICA", "ARDUINO"];
            localStorage.setItem('sopaDeLetrasWordBank', JSON.stringify(wordBank));
        }
    } catch (e) {
        console.error("Error loading word bank", e);
        wordBank = typeof defaultWordBank !== 'undefined' ? [...defaultWordBank] : ["ROBOTICA", "ARDUINO"];
    }
}

function addToWordBank(words) {
    let added = false;
    words.forEach(w => {
        if (!wordBank.includes(w)) {
            wordBank.push(w);
            added = true;
        }
    });
    
    if (added) {
        try {
            localStorage.setItem('sopaDeLetrasWordBank', JSON.stringify(wordBank));
        } catch (e) {
            console.error("Error saving word bank", e);
        }
    }
}

function fillRandomWords() {
    if (wordBank.length === 0) return;
    
    // Pick 12-18 random words
    const numWords = Math.floor(Math.random() * 7) + 12;
    
    const shuffled = [...wordBank].sort(() => 0.5 - Math.random());
    const selected = shuffled.slice(0, numWords);
    
    wordsInput.value = selected.join('\n');
    updateWordCount();
}

function showError(message) {
    errorToast.textContent = message;
    errorToast.classList.add('show');
    setTimeout(() => {
        errorToast.classList.remove('show');
    }, 4000);
}

function generatePDF() {
    const element = document.getElementById('pdf-content');
    const title = document.querySelector('.pdf-title');
    const pdfBtnElement = document.getElementById('pdf-btn');
    const originalText = pdfBtnElement.innerHTML;

    // Show title specifically for the PDF
    title.style.display = 'block';
    
    // Change button state
    pdfBtnElement.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 12a9 9 0 1 1-9-9c2.52 0 4.93 1 6.74 2.74L21 8"/><path d="M21 3v5h-5"/></svg> Generando...`;
    pdfBtnElement.disabled = true;

    // Configure PDF options
    const opt = {
        margin:       10,
        filename:     'Sopa_de_Letras.pdf',
        image:        { type: 'jpeg', quality: 0.98 },
        html2canvas:  { scale: 2, useCORS: true, backgroundColor: '#ffffff', windowWidth: 800 },
        jsPDF:        { unit: 'mm', format: 'a4', orientation: 'portrait' },
        pagebreak:    { mode: 'avoid-all' }
    };

    // Before generating, temporarily remove dark-mode specific CSS vars or classes if needed,
    // though html2canvas usually captures the computed styles as they look.
    // We add a temporary class to 'game-container' to force PDF styles similar to print
    element.classList.add('pdf-export-mode');

    // Generate PDF
    html2pdf().set(opt).from(element).save().then(() => {
        // Restore state
        title.style.display = 'none';
        element.classList.remove('pdf-export-mode');
        pdfBtnElement.innerHTML = originalText;
        pdfBtnElement.disabled = false;
    }).catch(err => {
        console.error("PDF generation error: ", err);
        showError("Ocurrió un error al generar el PDF.");
        title.style.display = 'none';
        element.classList.remove('pdf-export-mode');
        pdfBtnElement.innerHTML = originalText;
        pdfBtnElement.disabled = false;
    });
}

// Allowed directions based on difficulty
function getAllowedDirections() {
    const diff = difficultySelect.value;
    if (diff === 'easy') return DIRECTIONS.slice(0, 2); // only right, down
    if (diff === 'medium') return DIRECTIONS.slice(0, 4); // right, down, diags
    return DIRECTIONS; // all 8 directions
}

function calculateOptimalGridSize(words) {
    // Total characters
    const totalChars = words.reduce((sum, w) => sum + w.length, 0);
    // Longest word
    const maxLen = words.reduce((max, w) => Math.max(max, w.length), 0);
    
    // density factor (higher means tighter grid, but might fail)
    // For 30 words, we need a larger grid
    const targetArea = totalChars * 2.5; 
    let calcSize = Math.ceil(Math.sqrt(targetArea));
    
    // Must be at least as big as the longest word + 1
    calcSize = Math.max(calcSize, maxLen + 1);
    
    // Bounds
    calcSize = Math.max(10, Math.min(calcSize, 40));
    
    return calcSize;
}

function generatePuzzle() {
    isSolving = false;
    solveBtn.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12h10"/><path d="M9 4v16"/><path d="m3 9 3 3-3 3"/></svg> Mostrar Solución`;
    
    let words = getCleanWords();
    
    // Remove duplicates
    words = [...new Set(words)];

    // Add new words to word bank
    if (words.length > 0) {
        addToWordBank(words);
    }

    if (words.length === 0) {
        showError("Por favor, ingresa al menos una palabra válida (mínimo 2 letras).");
        return;
    }
    
    if (words.length > 30) {
        showError("Solo se permiten hasta 30 palabras. Tomando las primeras 30...");
        words = words.slice(0, 30);
    }

    // Determine grid size
    if (autoSizeCheckbox.checked) {
        gridSize = calculateOptimalGridSize(words);
    } else {
        gridSize = parseInt(gridSizeInput.value, 10);
        const maxLen = words.reduce((max, w) => Math.max(max, w.length), 0);
        if (gridSize < maxLen) {
            showError(`El tamaño de la cuadrícula (${gridSize}) es menor que la palabra más larga (${maxLen}). Autocambiando a ${maxLen + 1}.`);
            gridSize = maxLen + 1;
            gridSizeInput.value = gridSize;
            gridSizeDisplay.textContent = `${gridSize}x${gridSize}`;
        }
    }

    // Sort words by length descending (place longest first)
    words.sort((a, b) => b.length - a.length);

    generateGridLogic(words);
}

function createEmptyGrid() {
    const grid = [];
    for (let y = 0; y < gridSize; y++) {
        const row = [];
        for (let x = 0; x < gridSize; x++) {
            row.push('');
        }
        grid.push(row);
    }
    return grid;
}

function generateGridLogic(wordsToPlace) {
    let attempts = 0;
    const maxAttempts = 50; // Try different layouts
    let bestGrid = null;
    let bestPlacedCount = -1;
    let bestPlacedWords = [];

    const allowedDirections = getAllowedDirections();

    while (attempts < maxAttempts) {
        let tempGrid = createEmptyGrid();
        let currentPlaced = [];
        let allSuccess = true;

        for (const word of wordsToPlace) {
            const result = placeWord(word, tempGrid, allowedDirections);
            if (result) {
                currentPlaced.push(result);
            } else {
                allSuccess = false;
                // Continue trying other words even if one fails
            }
        }

        if (allSuccess) {
            bestGrid = tempGrid;
            bestPlacedWords = currentPlaced;
            break;
        }

        if (currentPlaced.length > bestPlacedCount) {
            bestPlacedCount = currentPlaced.length;
            bestGrid = tempGrid;
            bestPlacedWords = currentPlaced;
        }

        attempts++;
    }

    if (bestPlacedWords.length < wordsToPlace.length) {
        const missing = wordsToPlace.length - bestPlacedWords.length;
        showError(`No se pudieron colocar ${missing} palabra(s). Intenta aumentar el tamaño manual o cambiar la dificultad.`);
    }

    currentGrid = bestGrid;
    placedWords = bestPlacedWords;
    fillEmptySpaces();
    renderGrid();
    renderWordList();
    
    outputArea.style.display = 'flex';
    // Scroll to output
    setTimeout(() => {
        outputArea.scrollIntoView({ behavior: 'smooth' });
    }, 100);
}

function placeWord(word, grid, allowedDirections) {
    // Randomize directions available for this word
    // We shuffle allowedDirections
    const dirs = [...allowedDirections].sort(() => Math.random() - 0.5);
    
    // Try random positions
    let positions = [];
    for (let y = 0; y < gridSize; y++) {
        for (let x = 0; x < gridSize; x++) {
            positions.push({x, y});
        }
    }
    positions.sort(() => Math.random() - 0.5);

    for (const pos of positions) {
        for (const dir of dirs) {
            if (canPlaceWord(word, pos.x, pos.y, dir.x, dir.y, grid)) {
                // Place it
                const path = [];
                for (let i = 0; i < word.length; i++) {
                    const nx = pos.x + (dir.x * i);
                    const ny = pos.y + (dir.y * i);
                    grid[ny][nx] = word[i];
                    path.push({x: nx, y: ny});
                }
                return {
                    word: word,
                    origin: { x: pos.x, y: pos.y },
                    dir: { x: dir.x, y: dir.y },
                    path: path
                };
            }
        }
    }
    return null; // Could not place
}

function canPlaceWord(word, x, y, dx, dy, grid) {
    const endX = x + (dx * (word.length - 1));
    const endY = y + (dy * (word.length - 1));

    // Bounds check
    if (endX < 0 || endX >= gridSize || endY < 0 || endY >= gridSize) {
        return false;
    }

    // Overlap check
    for (let i = 0; i < word.length; i++) {
        const nx = x + (dx * i);
        const ny = y + (dy * i);
        const currentCell = grid[ny][nx];
        // Allow if cell is empty or has the exact same letter (intersection)
        if (currentCell !== '' && currentCell !== word[i]) {
            return false;
        }
    }
    
    return true;
}

function fillEmptySpaces() {
    for (let y = 0; y < gridSize; y++) {
        for (let x = 0; x < gridSize; x++) {
            if (currentGrid[y][x] === '') {
                // Random char
                currentGrid[y][x] = LETTERS.charAt(Math.floor(Math.random() * LETTERS.length));
            }
        }
    }
}

function toggleSolution() {
    isSolving = !isSolving;
    const cells = document.querySelectorAll('.grid-cell');
    
    if (isSolving) {
        solveBtn.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M22 13V6a2 2 0 0 0-2-2H4a2 2 0 0 0-2 2v12c0 1.1.9 2 2 2h8"/><path d="m22 7-8.97 5.7a1.94 1.94 0 0 1-2.06 0L2 7"/><path d="m16 19 2 2 4-4"/></svg> Ocultar Solución`;
        
        // Highlight all path cells
        placedWords.forEach(w => {
            w.path.forEach(p => {
                const idx = p.y * gridSize + p.x;
                cells[idx].classList.add('solution-highlight');
            });
        });
    } else {
        solveBtn.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12h10"/><path d="M9 4v16"/><path d="m3 9 3 3-3 3"/></svg> Mostrar Solución`;
        cells.forEach(c => c.classList.remove('solution-highlight'));
    }
}

// Rendering & Interaction
let isDragging = false;
let startCell = null;
let currentSelectionPath = [];
let selectionLine = null;

function renderGrid() {
    wordGridElement.innerHTML = '';
    // Set CSS grid property
    wordGridElement.style.gridTemplateColumns = `repeat(${gridSize}, 1fr)`;
    wordGridElement.style.setProperty('--grid-size', gridSize);
    
    // Clear found sets
    foundWordsSet.clear();
    
    for (let y = 0; y < gridSize; y++) {
        for (let x = 0; x < gridSize; x++) {
            const cell = document.createElement('div');
            cell.classList.add('grid-cell');
            cell.textContent = currentGrid[y][x];
            cell.dataset.x = x;
            cell.dataset.y = y;
            
            // Mouse/Touch events for playing
            cell.addEventListener('mousedown', handlePointerDown);
            cell.addEventListener('mouseenter', handlePointerEnter);
            
            // Touch support
            cell.addEventListener('touchstart', (e) => {
                e.preventDefault();
                handlePointerDown({ target: cell });
            });
            
            wordGridElement.appendChild(cell);
        }
    }
    
    // Global mouse up
    document.removeEventListener('mouseup', handlePointerUp);
    document.addEventListener('mouseup', handlePointerUp);
    
    // Touch move
    wordGridElement.addEventListener('touchmove', (e) => {
        e.preventDefault();
        if (!isDragging) return;
        
        const touch = e.touches[0];
        const elem = document.elementFromPoint(touch.clientX, touch.clientY);
        if (elem && elem.classList.contains('grid-cell')) {
            handlePointerEnter({ target: elem });
        }
    }, { passive: false });
    
    wordGridElement.addEventListener('touchend', handlePointerUp);
}

const foundWordsSet = new Set();

function renderWordList() {
    wordListElement.innerHTML = '';
    
    // Sort alphabetically for display
    const displayWords = [...placedWords].sort((a, b) => a.word.localeCompare(b.word));
    
    displayWords.forEach(w => {
        const li = document.createElement('li');
        li.classList.add('word-item');
        li.textContent = w.word;
        li.dataset.word = w.word;
        wordListElement.appendChild(li);
    });
}

// --- Interaction Logic ---

function handlePointerDown(e) {
    if (e.target.classList.contains('found')) return;
    
    isDragging = true;
    startCell = {
        x: parseInt(e.target.dataset.x, 10),
        y: parseInt(e.target.dataset.y, 10),
        elem: e.target
    };
    
    currentSelectionPath = [startCell];
    clearSelection();
    e.target.classList.add('selected');
}

function handlePointerEnter(e) {
    if (!isDragging || !startCell) return;
    
    const x = parseInt(e.target.dataset.x, 10);
    const y = parseInt(e.target.dataset.y, 10);
    
    // Check if it's a valid straight line
    const dx = Math.abs(x - startCell.x);
    const dy = Math.abs(y - startCell.y);
    
    // Must be horizontal, vertical, or perfectly diagonal
    if (x === startCell.x || y === startCell.y || dx === dy) {
        clearSelection();
        currentSelectionPath = [];
        
        const stepX = x === startCell.x ? 0 : (x > startCell.x ? 1 : -1);
        const stepY = y === startCell.y ? 0 : (y > startCell.y ? 1 : -1);
        
        const distance = Math.max(dx, dy);
        
        const cells = document.querySelectorAll('.grid-cell');
        
        for (let i = 0; i <= distance; i++) {
            const cx = startCell.x + (stepX * i);
            const cy = startCell.y + (stepY * i);
            const idx = cy * gridSize + cx;
            const elem = cells[idx];
            
            elem.classList.add('selected');
            currentSelectionPath.push({x: cx, y: cy, elem});
        }
    }
}

function handlePointerUp() {
    if (!isDragging) return;
    isDragging = false;
    
    if (currentSelectionPath.length > 1) {
        checkSelection();
    }
    
    // Clear selection UI
    setTimeout(() => {
        clearSelection();
    }, 200);
}

function clearSelection() {
    document.querySelectorAll('.grid-cell.selected').forEach(c => c.classList.remove('selected'));
}

function checkSelection() {
    // Get string from path
    let selectedWord = currentSelectionPath.map(p => currentGrid[p.y][p.x]).join('');
    let reversedSelected = selectedWord.split('').reverse().join('');
    
    // Check if it matches any placed word
    const match = placedWords.find(pw => 
        (pw.word === selectedWord || pw.word === reversedSelected) &&
        !foundWordsSet.has(pw.word)
    );
    
    if (match) {
        // Mark as found
        foundWordsSet.add(match.word);
        
        // Update UI grid
        currentSelectionPath.forEach(p => {
            p.elem.classList.add('found');
        });
        
        // If they selected it backward, it's fine, the path matches the cells visually
        
        // Update list UI
        const li = document.querySelector(`.word-item[data-word="${match.word}"]`);
        if (li) li.classList.add('found');
        
        // Check win condition
        if (foundWordsSet.size === placedWords.length) {
            setTimeout(() => {
                alert("¡Felicidades! Has completado la sopa de letras.");
            }, 300);
        }
    }
}
