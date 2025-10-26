#!/usr/bin/env python3
"""
ab_project_create - Create audio-bench project directory structures

This script creates organized project directories for audio-bench workflows.
Currently supports 'device_report' project type, with more types planned.
"""

import argparse
import os
import sys
from datetime import datetime
from pathlib import Path


# Supported project types
PROJECT_TYPES = {
    'device_report': {
        'description': 'Device testing and reporting project',
        'subdirs': ['data', 'scripts', 'reports']
    }
    # Future project types can be added here
}


def create_project_structure(project_type, project_dir):
    """
    Create the project directory structure.

    Args:
        project_type: Type of project to create (e.g., 'device_report')
        project_dir: Path where the project will be created

    Returns:
        True if successful, False otherwise
    """
    project_path = Path(project_dir)

    # Check if directory already exists
    if project_path.exists():
        print(f"Error: Directory '{project_dir}' already exists", file=sys.stderr)
        return False

    # Get project configuration
    config = PROJECT_TYPES[project_type]

    try:
        # Create main project directory
        print(f"Creating project directory: {project_dir}")
        project_path.mkdir(parents=True, exist_ok=False)

        # Create subdirectories
        for subdir in config['subdirs']:
            subdir_path = project_path / subdir
            print(f"  Creating subdirectory: {subdir}/")
            subdir_path.mkdir(exist_ok=False)

        # Create README.md
        readme_path = project_path / 'README.md'
        creation_date = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        readme_content = f"""# audio-bench Project Directory

This is an audio-bench project directory.

**Project Type:** {project_type}
**Created:** {creation_date}

## Directory Structure

- **data/**: Raw data files (WAV files, measurements, etc.)
- **scripts/**: Python scripts for report generation and analysis
- **reports/**: Generated output reports and visualizations

## Usage

This project was created for {config['description'].lower()}.

Use audio-bench tools to populate the data directory, then run analysis scripts
to generate reports in the reports directory.

For more information, see the audio-bench documentation.
"""

        print(f"  Creating README.md")
        with open(readme_path, 'w', encoding='utf-8') as f:
            f.write(readme_content)

        print(f"\nProject '{project_dir}' created successfully!")
        print(f"Project type: {project_type}")

        return True

    except OSError as e:
        print(f"Error creating project structure: {e}", file=sys.stderr)
        # Attempt cleanup if partial creation occurred
        if project_path.exists():
            try:
                import shutil
                shutil.rmtree(project_path)
                print("Cleaned up partial project creation", file=sys.stderr)
            except Exception:
                pass
        return False


def main():
    parser = argparse.ArgumentParser(
        description='Create audio-bench project directory structure',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s device_report my_audio_test/
  %(prog)s device_report ../projects/speaker_analysis/

Supported project types:
  device_report    Device testing and reporting project
        """
    )

    parser.add_argument(
        'project_type',
        choices=PROJECT_TYPES.keys(),
        help='Type of project to create'
    )

    parser.add_argument(
        'project_dir',
        help='Directory path where project will be created'
    )

    parser.add_argument(
        '--list-types',
        action='store_true',
        help='List available project types and exit'
    )

    args = parser.parse_args()

    # Handle --list-types
    if args.list_types:
        print("Available project types:")
        for ptype, config in PROJECT_TYPES.items():
            print(f"  {ptype:20} - {config['description']}")
        sys.exit(0)

    # Validate project type (should be caught by choices, but double-check)
    if args.project_type not in PROJECT_TYPES:
        print(f"Error: Unknown project type '{args.project_type}'", file=sys.stderr)
        print(f"Available types: {', '.join(PROJECT_TYPES.keys())}", file=sys.stderr)
        sys.exit(1)

    # Create the project
    success = create_project_structure(args.project_type, args.project_dir)

    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
