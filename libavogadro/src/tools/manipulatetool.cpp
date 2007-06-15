/**********************************************************************
  ManipulateTool - Manipulation Tool for Avogadro

  Copyright (C) 2007 by Marcus D. Hanwell
  Copyright (C) 2007 by Geoffrey R. Hutchison
  Copyright (C) 2007 by Benoit Jacob

  This file is part of the Avogadro molecular editor project.
  For more information, see <http://avogadro.sourceforge.net/>

  Some code is based on Open Babel
  For more information, see <http://openbabel.sourceforge.net/>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 ***********************************************************************/

#include "manipulatetool.h"
// #include "moveatomcommand.h"
#include <avogadro/primitive.h>
#include <avogadro/color.h>
#include <avogadro/glwidget.h>
#include <avogadro/camera.h>

#include <openbabel/obiter.h>
#include <openbabel/mol.h>

#include <QtPlugin>

using namespace std;
using namespace OpenBabel;
using namespace Avogadro;
using namespace Eigen;

ManipulateTool::ManipulateTool(QObject *parent) : Tool(parent), m_clickedAtom(0), m_leftButtonPressed(false), m_midButtonPressed(false), m_rightButtonPressed(false), m_undo(0)
{
  QAction *action = activateAction();
  action->setIcon(QIcon(QString::fromUtf8(":/manipulate/manipulate.png")));
  action->setToolTip(tr("Manipulation Tool (F10)\n\n"
        "Left Mouse:   Click and drag to move atoms\n"
        "Middle Mouse: Click and drag to move atoms further away or closer\n"
        "Right Mouse:  Click and drag to rotate selected atoms."));
  action->setShortcut(Qt::Key_F10);
}

ManipulateTool::~ManipulateTool()
{
}

int ManipulateTool::usefulness() const
{
  return 600000;
}

void ManipulateTool::computeClickedAtom(const QPoint& p)
{
  QList<GLHit> hits;
  m_clickedAtom = 0;

  // Perform a OpenGL selection and retrieve the list of hits.
  hits = m_glwidget->hits(p.x()-SEL_BOX_HALF_SIZE,
      p.y()-SEL_BOX_HALF_SIZE,
      SEL_BOX_SIZE, SEL_BOX_SIZE);

  // Find the first atom (if any) in hits - this will be the closest
  foreach( GLHit hit, hits )
  {
    if(hit.type() == Primitive::AtomType)
    {
      m_clickedAtom = static_cast<Atom *>( m_glwidget->molecule()->GetAtom(hit.name()) );
      return;
    }
  }
}

void ManipulateTool::zoom( const Eigen::Vector3d &goal, double delta ) const
{
  // Move the selected atom(s) in to or out of the screen
  MoveAtomCommand *cmd  = 0;
  Vector3d transformedGoal = m_glwidget->camera()->modelview() * goal;
  double distanceToGoal = transformedGoal.norm();

  double t = ZOOM_SPEED * delta;
  const double minDistanceToGoal = 2.0 * CAMERA_NEAR_DISTANCE;
  double u = minDistanceToGoal / distanceToGoal - 1.0;

  if( fabs(t) > fabs(u) ) {
    t = u;
  }

  MatrixP3d atomTranslation;
  atomTranslation.loadTranslation(m_glwidget->camera()->backtransformedZAxis() * t);

  if (m_glwidget->selectedPrimitives().size())
  {
    foreach(Primitive *a, m_glwidget->selectedPrimitives())
    {
      if (a->type() == Primitive::AtomType)
      {
        Atom *atom = static_cast<Atom *>(a);
        cmd = new MoveAtomCommand(m_glwidget->molecule(), atom, atomTranslation * atom->pos(), m_undo);
      }
    }
  }
  if (m_clickedAtom && !m_glwidget->isSelected(m_clickedAtom))
    cmd = new MoveAtomCommand(m_glwidget->molecule(), m_clickedAtom, atomTranslation * m_clickedAtom->pos(), m_undo);
}

void ManipulateTool::translate( const Eigen::Vector3d &what, const QPoint &from, const QPoint &to ) const
{
  // Translate the selected atoms in the x and y sense of the view
  MoveAtomCommand *cmd  = 0;
  Vector3d fromPos = m_glwidget->camera()->unProject(from, what);
  Vector3d toPos = m_glwidget->camera()->unProject(to, what);

  MatrixP3d atomTranslation;
  atomTranslation.loadTranslation(toPos - fromPos);

  if (m_glwidget->selectedPrimitives().size())
  {
    foreach(Primitive *a, m_glwidget->selectedPrimitives())
    {
      if (a->type() == Primitive::AtomType)
      {
        Atom *atom = static_cast<Atom *>(a);
        cmd = new MoveAtomCommand(m_glwidget->molecule(), atom, atomTranslation * atom->pos(), m_undo);
      }
    }
  }
  if (m_clickedAtom && !m_glwidget->isSelected(m_clickedAtom))
    cmd = new MoveAtomCommand(m_glwidget->molecule(), m_clickedAtom, atomTranslation * m_clickedAtom->pos(), m_undo);
}

void ManipulateTool::rotate( const Eigen::Vector3d &center, double deltaX, double deltaY ) const
{
  // Rotate the selected atoms about the center
  MoveAtomCommand *cmd  = 0;
  Matrix3d rotation = m_glwidget->camera()->modelview().linearComponent();
  Vector3d XAxis = rotation.row(0);
  Vector3d YAxis = rotation.row(1);
  // rotate only selected primitives
  MatrixP3d fragmentRotation;
  fragmentRotation.loadTranslation(center);
  fragmentRotation.rotate3(deltaY * ROTATION_SPEED, XAxis );
  fragmentRotation.rotate3(deltaX * ROTATION_SPEED, YAxis );
  fragmentRotation.translate(-center);

  foreach(Primitive *a, m_glwidget->selectedPrimitives())
  {
    if (a->type() == Primitive::AtomType)
    {
      Atom *atom = static_cast<Atom *>(a);
      cmd = new MoveAtomCommand(m_glwidget->molecule(), atom, fragmentRotation * atom->pos(), m_undo);
    }
  }
}

void ManipulateTool::tilt( const Eigen::Vector3d &center, double delta ) const
{
  // Tilt the selected atoms about the center
  MoveAtomCommand *cmd  = 0;
  MatrixP3d fragmentRotation;
  fragmentRotation.loadTranslation(center);
  fragmentRotation.rotate3(delta * ROTATION_SPEED, m_glwidget->camera()->backtransformedZAxis());
  fragmentRotation.translate(-center);
  foreach(Primitive *a, m_glwidget->selectedPrimitives())
  {
    if (a->type() == Primitive::AtomType)
    {
      Atom *atom = static_cast<Atom *>(a);
      cmd = new MoveAtomCommand(m_glwidget->molecule(), atom, fragmentRotation * atom->pos(), m_undo);
    }
  }
}

QUndoCommand* ManipulateTool::mousePress(GLWidget *widget, const QMouseEvent *event)
{
  m_glwidget = widget;
  m_lastDraggingPosition = event->pos();
  m_leftButtonPressed = ( event->buttons() & Qt::LeftButton );
  m_midButtonPressed = ( event->buttons() & Qt::MidButton );
  m_rightButtonPressed = ( event->buttons() & Qt::RightButton );
  computeClickedAtom(event->pos());

  widget->update();
  m_undo = new MoveAtomCommand(m_glwidget->molecule());
  return m_undo;
}

QUndoCommand* ManipulateTool::mouseRelease(GLWidget *widget, const QMouseEvent*)
{
  m_glwidget = widget;
  m_leftButtonPressed = false;
  m_midButtonPressed = false;
  m_rightButtonPressed = false;
  m_clickedAtom = 0;

  widget->update();
  return 0;
}

QUndoCommand* ManipulateTool::mouseMove(GLWidget *widget, const QMouseEvent *event)
{
  m_glwidget = widget;
  if(!m_glwidget->molecule()) {
    return 0;
  }
  m_undo = new MoveAtomCommand(m_glwidget->molecule());

  // Get the currently selected atoms from the view
  QList<Primitive *> currentSelection = m_glwidget->selectedPrimitives();

  QPoint deltaDragging = event->pos() - m_lastDraggingPosition;

  // Manipulation can be performed in two ways - centred on an individual atom

  if (m_clickedAtom)
  {
    if ( event->buttons() & Qt::LeftButton )
    {
      // Atom centred rotation
      rotate( m_clickedAtom->pos(), deltaDragging.x(), deltaDragging.y() );
    }
    else if ( event->buttons() & Qt::MidButton )
    {
      // Perform the rotation
      tilt( m_clickedAtom->pos(), deltaDragging.x() );

      // Perform the zoom toward clicked atom
      zoom( m_clickedAtom->pos(), deltaDragging.y() );
    }
    else if ( event->buttons() & Qt::RightButton )
    {
      // translate the molecule following mouse movement
      translate( m_clickedAtom->pos(), m_lastDraggingPosition, event->pos() );
    }
  }
  else if (currentSelection.size())
  {
    // Some atoms are selected - work out where the center is
    m_selectedPrimitivesCenter.loadZero();
    foreach(Primitive *hit, currentSelection)
    {
      if (hit->type() == Primitive::AtomType)
      {
        Atom *atom = static_cast<Atom *>(hit);
        m_selectedPrimitivesCenter += atom->pos();
      }
    }
    m_selectedPrimitivesCenter /= currentSelection.size();

    if ( event->buttons() & Qt::LeftButton )
    {
      // rotation around the center of the selected atoms
      rotate( m_selectedPrimitivesCenter, deltaDragging.x(), deltaDragging.y() );
    }
    else if ( event->buttons() & Qt::MidButton )
    {
      // Perform the rotation
      tilt( m_selectedPrimitivesCenter, deltaDragging.x() );

      // Perform the zoom toward molecule center
      zoom( m_selectedPrimitivesCenter, deltaDragging.y() );
    }
    else if( event->buttons() & Qt::RightButton )
    {
      // translate the molecule following mouse movement
      translate( m_selectedPrimitivesCenter, m_lastDraggingPosition, event->pos() );
    }
  }

  m_lastDraggingPosition = event->pos();
  m_glwidget->update();

  return m_undo;
}

QUndoCommand* ManipulateTool::wheel(GLWidget*, const QWheelEvent*)
{
  return 0;
}

bool ManipulateTool::paint(GLWidget *widget)
{
  if(m_leftButtonPressed || m_midButtonPressed || m_rightButtonPressed) {
    if( m_clickedAtom && (!m_leftButtonPressed || m_glwidget->selectedPrimitives().size()) )
    {
      // Don't highlight the atom on right mouse unless there is a selection
      double renderRadius = 0.0;
      foreach(Engine *engine, widget->engines())
      {
        if(engine->isEnabled())
        {
          double engineRadius = engine->radius(m_clickedAtom);
          if(engineRadius > renderRadius) {
            renderRadius = engineRadius;
          }
        }
      }
      renderRadius += 0.10;
      drawSphere(widget, m_clickedAtom->GetVector().AsArray(), renderRadius, 0.7);
    }
    else if (m_glwidget->selectedPrimitives().size())
    {
      // Only draw the central sphere if something is selected
      drawSphere(widget, m_selectedPrimitivesCenter, 0.10, 1.0);
    }
  }
  return true;
}

void ManipulateTool::drawSphere(GLWidget *widget,  const Eigen::Vector3d &position, double radius, float alpha )
{
  widget->painter()->begin(widget);
  Color( 1.0, 0.3, 0.3, alpha ).applyAsMaterials();
  glEnable( GL_BLEND );
  widget->painter()->drawSphere(position, radius);
  glDisable( GL_BLEND );
  widget->painter()->end();
}

MoveAtomCommand::MoveAtomCommand(Molecule *molecule, QUndoCommand *parent) : QUndoCommand(parent), m_molecule(0)
{
  // Store the molecule - this call won't actually move an atom
  setText(QObject::tr("Manipulate Atom"));
  m_moleculeCopy = *molecule;
  m_molecule = molecule;
  m_atomIndex = 0;
  undone = false;
}

MoveAtomCommand::MoveAtomCommand(Molecule *molecule, Atom *atom, Eigen::Vector3d pos, QUndoCommand *parent) : QUndoCommand(parent), m_molecule(0)
{
  // Store the original molecule before any modifications are made
  setText(QObject::tr("Manipulate Atom"));
  m_moleculeCopy = *molecule;
  m_molecule = molecule;
  m_atomIndex = atom->GetIdx();
  m_pos = pos;
  undone = false;
}

void MoveAtomCommand::redo()
{
  // Move the specified atom to the location given
  if (undone)
  {
    Molecule newMolecule = *m_molecule;
    *m_molecule = m_moleculeCopy;
    m_moleculeCopy = newMolecule;
  }
  else if (m_atomIndex)
  {
    m_molecule->BeginModify();
    Atom *atom = static_cast<Atom *>(m_molecule->GetAtom(m_atomIndex));
    atom->setPos(m_pos);
    m_molecule->EndModify();
    atom->update();
  }
  QUndoCommand::redo();
}

void MoveAtomCommand::undo()
{
  // Restore our original molecule
  Molecule newMolecule = *m_molecule;
  *m_molecule = m_moleculeCopy;
  m_moleculeCopy = newMolecule;
  undone = true;
}

bool MoveAtomCommand::mergeWith (const QUndoCommand *)
{
  // Just return true to repeated calls - we have stored the original molecule
  return true;
}

int MoveAtomCommand::id() const
{
  return 26011980;
}

#include "manipulatetool.moc"

Q_EXPORT_PLUGIN2(manipulatetool, ManipulateToolFactory)
